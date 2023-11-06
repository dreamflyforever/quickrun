/*
 * Copyright 2015 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#if defined(_WIN32)
#include "vld.h"
#endif

#define MODULE_TAG "mpi_dec_test"

#include <string.h>
#include "rk_mpi.h"

#include "mpp_mem.h"
#include "mpp_env.h"
#include "mpp_time.h"
#include "mpp_common.h"

#include "utils.h"

#define MPI_DEC_LOOP_COUNT          4
#define MPI_DEC_STREAM_SIZE         (SZ_4K)
#define MAX_FILE_NAME_LENGTH        256

#define mpp_assert assert
typedef struct {
    MppCtx          ctx;
    MppApi          *mpi;

    /* end of stream flag when set quit the loop */
    RK_U32          eos;

    /* buffer for stream data reading */
    char            *buf;

    /* input and output */
    MppBufferGroup  frm_grp;
    MppBufferGroup  pkt_grp;
    MppPacket       packet;
    size_t          packet_size;
    MppFrame        frame;

    FILE            *fp_input;
    FILE            *fp_output;
    FILE            *fp_config;
    RK_S32          frame_count;
    RK_S32          frame_num;
    size_t          max_usage;
} MpiDecLoopData;

typedef struct {
    char            file_input[MAX_FILE_NAME_LENGTH];
    char            file_output[MAX_FILE_NAME_LENGTH];
    char            file_config[MAX_FILE_NAME_LENGTH];
    MppCodingType   type;
    MppFrameFormat  format;
    RK_U32          width;
    RK_U32          height;
    RK_U32          debug;

    RK_U32          have_input;
    RK_U32          have_output;
    RK_U32          have_config;

    RK_U32          simple;
    RK_S32          timeout;
    RK_S32          frame_num;
    size_t          pkt_size;

    // report information
    size_t          max_usage;
} MpiDecTestCmd;

static OptionInfo mpi_dec_cmd[] = {
    {"i",               "input_file",           "input bitstream file"},
    {"o",               "output_file",          "output bitstream file, "},
    {"c",               "ops_file",             "input operation config file"},
    {"w",               "width",                "the width of input bitstream"},
    {"h",               "height",               "the height of input bitstream"},
    {"t",               "type",                 "input stream coding type"},
    {"f",               "format",               "output frame format type"},
    {"d",               "debug",                "debug flag"},
    {"x",               "timeout",              "output timeout interval"},
    {"n",               "frame_number",         "max output frame number"},
};

static int decode_simple(MpiDecLoopData *data)
{
    RK_U32 pkt_done = 0;
    RK_U32 pkt_eos  = 0;
    MPP_RET ret = MPP_OK;
    MppCtx ctx  = data->ctx;
    MppApi *mpi = data->mpi;
    char   *buf = data->buf;
    MppPacket packet = data->packet;
    MppFrame  frame  = NULL;
    size_t read_size = 0;
    size_t packet_size = data->packet_size;

    do {
        if (data->fp_config) {
            char line[MAX_FILE_NAME_LENGTH];
            char *ptr = NULL;

            do {
                ptr = fgets(line, MAX_FILE_NAME_LENGTH, data->fp_config);
                if (ptr) {
                    OpsLine info;
                    RK_S32 cnt = parse_config_line(line, &info);

                    // parser for packet message
                    if (cnt >= 3 && 0 == strncmp("pkt", info.cmd, sizeof(info.cmd))) {
                        packet_size = info.value2;
                        break;
                    }

                    // parser for reset message at the end
                    if (0 == strncmp("rst", info.cmd, 3)) {
                        printf("%p get reset cmd\n", ctx);
                        packet_size = 0;
                        break;
                    }
                } else {
                    printf("%p get end of cfg file\n", ctx);
                    packet_size = 0;
                    break;
                }
            } while (1);
        }

        // when packet size is valid read the input binary file
        if (packet_size)
            read_size = fread(buf, 1, packet_size, data->fp_input);

        if (!packet_size || read_size != packet_size || feof(data->fp_input)) {
            if (data->frame_num < 0) {
                clearerr(data->fp_input);
                rewind(data->fp_input);
                if (data->fp_config) {
                    clearerr(data->fp_config);
                    rewind(data->fp_config);
                }
                data->eos = pkt_eos = 0;
                printf("%p loop again\n", ctx);
            } else {
                // setup eos flag
                data->eos = pkt_eos = 1;
                printf("%p found last packet\n", ctx);
                break;
            }
        }
    } while (!read_size);

    // write data to packet
    mpp_packet_write(packet, 0, buf, read_size);
    // reset pos and set valid length
    mpp_packet_set_pos(packet, buf);
    mpp_packet_set_length(packet, read_size);
    // setup eos flag
    if (pkt_eos)
        mpp_packet_set_eos(packet);

    do {
        RK_S32 times = 5;
        // send the packet first if packet is not done
        if (!pkt_done) {
            ret = mpi->decode_put_packet(ctx, packet);
            if (MPP_OK == ret)
                pkt_done = 1;
        }

        // then get all available frame and release
        do {
            RK_S32 get_frm = 0;
            RK_U32 frm_eos = 0;

        try_again:
            ret = mpi->decode_get_frame(ctx, &frame);
            if (MPP_ERR_TIMEOUT == ret) {
                if (times > 0) {
                    times--;
                    msleep(2);
                    goto try_again;
                }
                printf("%p decode_get_frame failed too much time\n", ctx);
            }
            if (MPP_OK != ret) {
                printf("%p decode_get_frame failed ret %d\n", ret, ctx);
                break;
            }

            if (frame) {
                if (mpp_frame_get_info_change(frame)) {
                    RK_U32 width = mpp_frame_get_width(frame);
                    RK_U32 height = mpp_frame_get_height(frame);
                    RK_U32 hor_stride = mpp_frame_get_hor_stride(frame);
                    RK_U32 ver_stride = mpp_frame_get_ver_stride(frame);
                    RK_U32 buf_size = mpp_frame_get_buf_size(frame);

                    printf("%p decode_get_frame get info changed found\n", ctx);
                    printf("%p decoder require buffer w:h [%d:%d] stride [%d:%d] buf_size %d",
                           ctx, width, height, hor_stride, ver_stride, buf_size);

                    /*
                     * NOTE: We can choose decoder's buffer mode here.
                     * There are three mode that decoder can support:
                     *
                     * Mode 1: Pure internal mode
                     * In the mode user will NOT call MPP_DEC_SET_EXT_BUF_GROUP
                     * control to decoder. Only call MPP_DEC_SET_INFO_CHANGE_READY
                     * to let decoder go on. Then decoder will use create buffer
                     * internally and user need to release each frame they get.
                     *
                     * Advantage:
                     * Easy to use and get a demo quickly
                     * Disadvantage:
                     * 1. The buffer from decoder may not be return before
                     * decoder is close. So memroy leak or crash may happen.
                     * 2. The decoder memory usage can not be control. Decoder
                     * is on a free-to-run status and consume all memory it can
                     * get.
                     * 3. Difficult to implement zero-copy display path.
                     *
                     * Mode 2: Half internal mode
                     * This is the mode current test code using. User need to
                     * create MppBufferGroup according to the returned info
                     * change MppFrame. User can use mpp_buffer_group_limit_config
                     * function to limit decoder memory usage.
                     *
                     * Advantage:
                     * 1. Easy to use
                     * 2. User can release MppBufferGroup after decoder is closed.
                     *    So memory can stay longer safely.
                     * 3. Can limit the memory usage by mpp_buffer_group_limit_config
                     * Disadvantage:
                     * 1. The buffer limitation is still not accurate. Memory usage
                     * is 100% fixed.
                     * 2. Also difficult to implement zero-copy display path.
                     *
                     * Mode 3: Pure external mode
                     * In this mode use need to create empty MppBufferGroup and
                     * import memory from external allocator by file handle.
                     * On Android surfaceflinger will create buffer. Then
                     * mediaserver get the file handle from surfaceflinger and
                     * commit to decoder's MppBufferGroup.
                     *
                     * Advantage:
                     * 1. Most efficient way for zero-copy display
                     * Disadvantage:
                     * 1. Difficult to learn and use.
                     * 2. Player work flow may limit this usage.
                     * 3. May need a external parser to get the correct buffer
                     * size for the external allocator.
                     *
                     * The required buffer size caculation:
                     * hor_stride * ver_stride * 3 / 2 for pixel data
                     * hor_stride * ver_stride / 2 for extra info
                     * Total hor_stride * ver_stride * 2 will be enough.
                     *
                     * For H.264/H.265 20+ buffers will be enough.
                     * For other codec 10 buffers will be enough.
                     */

                    if (NULL == data->frm_grp) {
                        /* If buffer group is not set create one and limit it */
                        ret = mpp_buffer_group_get_internal(&data->frm_grp, MPP_BUFFER_TYPE_ION);
                        if (ret) {
                            printf("%p get mpp buffer group failed ret %d\n", ctx, ret);
                            break;
                        }

                        /* Set buffer to mpp decoder */
                        ret = mpi->control(ctx, MPP_DEC_SET_EXT_BUF_GROUP, data->frm_grp);
                        if (ret) {
                            printf("%p set buffer group failed ret %d\n", ctx, ret);
                            break;
                        }
                    } else {
                        /* If old buffer group exist clear it */
                        ret = mpp_buffer_group_clear(data->frm_grp);
                        if (ret) {
                            printf("%p clear buffer group failed ret %d\n", ctx, ret);
                            break;
                        }
                    }

                    /* Use limit config to limit buffer count to 24 with buf_size */
                    ret = mpp_buffer_group_limit_config(data->frm_grp, buf_size, 24);
                    if (ret) {
                        printf("%p limit buffer group failed ret %d\n", ctx, ret);
                        break;
                    }

                    /*
                     * All buffer group config done. Set info change ready to let
                     * decoder continue decoding
                     */
                    ret = mpi->control(ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
                    if (ret) {
                        printf("%p info change ready failed ret %d\n", ctx, ret);
                        break;
                    }
                } else {
                    char log_buf[256];
                    RK_S32 log_size = sizeof(log_buf) - 1;
                    RK_S32 log_len = 0;
                    RK_U32 err_info = mpp_frame_get_errinfo(frame);
                    RK_U32 discard = mpp_frame_get_discard(frame);

                    log_len += snprintf(log_buf + log_len, log_size - log_len,
                                        "decode get frame %d", data->frame_count);

                    if (mpp_frame_has_meta(frame)) {
                        MppMeta meta = mpp_frame_get_meta(frame);
                        RK_S32 temporal_id = 0;

                        mpp_meta_get_s32(meta, KEY_TEMPORAL_ID, &temporal_id);

                        log_len += snprintf(log_buf + log_len, log_size - log_len,
                                            " tid %d", temporal_id);
                    }

                    if (err_info || discard) {
                        log_len += snprintf(log_buf + log_len, log_size - log_len,
                                            " err %x discard %x", err_info, discard);
                    }
                    printf("%p %s\n", ctx, log_buf);

                    data->frame_count++;
                    if (data->fp_output && !err_info)
                        dump_mpp_frame_to_file(frame, data->fp_output);
                }
                frm_eos = mpp_frame_get_eos(frame);
                mpp_frame_deinit(&frame);
                frame = NULL;
                get_frm = 1;
            }

            // try get runtime frame memory usage
            if (data->frm_grp) {
                size_t usage = mpp_buffer_group_usage(data->frm_grp);
                if (usage > data->max_usage)
                    data->max_usage = usage;
            }

            // if last packet is send but last frame is not found continue
            if (pkt_eos && pkt_done && !frm_eos) {
                msleep(10);
                continue;
            }

            if (frm_eos) {
                printf("%p found last packet\n", ctx);
                break;
            }

            if (data->frame_num > 0 && data->frame_count >= data->frame_num) {
                data->eos = 1;
                break;
            }

            if (get_frm)
                continue;
            break;
        } while (1);

        if (data->frame_num > 0 && data->frame_count >= data->frame_num) {
            data->eos = 1;
            printf("%p reach max frame number %d\n", ctx, data->frame_count);
            break;
        }

        if (pkt_done)
            break;

        /*
         * why sleep here:
         * mpi->decode_put_packet will failed when packet in internal queue is
         * full,waiting the package is consumed .Usually hardware decode one
         * frame which resolution is 1080p needs 2 ms,so here we sleep 3ms
         * * is enough.
         */
        msleep(3);
    } while (1);

    return ret;
}

static int decode_advanced(MpiDecLoopData *data)
{
    RK_U32 pkt_eos  = 0;
    MPP_RET ret = MPP_OK;
    MppCtx ctx  = data->ctx;
    MppApi *mpi = data->mpi;
    char   *buf = data->buf;
    MppPacket packet = data->packet;
    MppFrame  frame  = data->frame;
    MppTask task = NULL;
    size_t read_size = fread(buf, 1, data->packet_size, data->fp_input);

    if (read_size != data->packet_size || feof(data->fp_input)) {
        printf("%p found last packet\n", ctx);

        // setup eos flag
        data->eos = pkt_eos = 1;
    }

    // reset pos
    mpp_packet_set_pos(packet, buf);
    mpp_packet_set_length(packet, read_size);
    // setup eos flag
    if (pkt_eos)
        mpp_packet_set_eos(packet);

    ret = mpi->poll(ctx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (ret) {
        printf("%p mpp input poll failed\n", ctx);
        return ret;
    }

    ret = mpi->dequeue(ctx, MPP_PORT_INPUT, &task);  /* input queue */
    if (ret) {
        printf("%p mpp task input dequeue failed\n", ctx);
        return ret;
    }

    mpp_assert(task);

    mpp_task_meta_set_packet(task, KEY_INPUT_PACKET, packet);
    mpp_task_meta_set_frame (task, KEY_OUTPUT_FRAME,  frame);

    ret = mpi->enqueue(ctx, MPP_PORT_INPUT, task);  /* input queue */
    if (ret) {
        printf("%p mpp task input enqueue failed\n", ctx);
        return ret;
    }

    /* poll and wait here */
    ret = mpi->poll(ctx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
    if (ret) {
        printf("%p mpp output poll failed\n", ctx);
        return ret;
    }

    ret = mpi->dequeue(ctx, MPP_PORT_OUTPUT, &task); /* output queue */
    if (ret) {
        printf("%p mpp task output dequeue failed\n", ctx);
        return ret;
    }

    mpp_assert(task);

    if (task) {
        MppFrame frame_out = NULL;
        mpp_task_meta_get_frame(task, KEY_OUTPUT_FRAME, &frame_out);
        //mpp_assert(packet_out == packet);

        if (frame) {
            /* write frame to file here */
            if (data->fp_output)
                dump_mpp_frame_to_file(frame, data->fp_output);

            printf("%p decoded frame %d\n", ctx, data->frame_count);
            data->frame_count++;

            if (mpp_frame_get_eos(frame_out))
                printf("%p found eos frame\n", ctx);
        }

        /* output queue */
        ret = mpi->enqueue(ctx, MPP_PORT_OUTPUT, task);
        if (ret)
            printf("%p mpp task output enqueue failed\n", ctx);
    }

    return ret;
}

int mpi_dec_test_decode(MpiDecTestCmd *cmd)
{
    MPP_RET ret         = MPP_OK;
    size_t file_size    = 0;

    // base flow context
    MppCtx ctx          = NULL;
    MppApi *mpi         = NULL;

    // input / output
    MppPacket packet    = NULL;
    MppFrame  frame     = NULL;

    MpiCmd mpi_cmd      = MPP_CMD_BASE;
    MppParam param      = NULL;
    RK_U32 need_split   = 1;
    MppPollType timeout = cmd->timeout;

    // paramter for resource malloc
    RK_U32 width        = cmd->width;
    RK_U32 height       = cmd->height;
    MppCodingType type  = cmd->type;

    // resources
    char *buf           = NULL;
    size_t packet_size  = cmd->pkt_size;
    MppBuffer pkt_buf   = NULL;
    MppBuffer frm_buf   = NULL;

    MpiDecLoopData data;

    printf("mpi_dec_test start\n");
    memset(&data, 0, sizeof(data));

    if (cmd->have_input) {
        data.fp_input = fopen(cmd->file_input, "rb");
        if (NULL == data.fp_input) {
            printf("failed to open input file %s\n", cmd->file_input);
            goto MPP_TEST_OUT;
        }

        fseek(data.fp_input, 0L, SEEK_END);
        file_size = ftell(data.fp_input);
        rewind(data.fp_input);
        printf("input file size %ld\n", file_size);
    }

    if (cmd->have_output) {
        data.fp_output = fopen(cmd->file_output, "w+b");
        if (NULL == data.fp_output) {
            printf("failed to open output file %s\n", cmd->file_output);
            goto MPP_TEST_OUT;
        }
    }

    if (cmd->have_config) {
        data.fp_config = fopen(cmd->file_config, "r");
        if (NULL == data.fp_config) {
            printf("failed to open config file %s\n", cmd->file_config);
            goto MPP_TEST_OUT;
        }
    }

    if (cmd->simple) {
        buf = mpp_malloc(char, packet_size);
        if (NULL == buf) {
            printf("mpi_dec_test malloc input stream buffer failed\n");
            goto MPP_TEST_OUT;
        }

        ret = mpp_packet_init(&packet, buf, packet_size);
        if (ret) {
            printf("mpp_packet_init failed\n");
            goto MPP_TEST_OUT;
        }
    } else {
        RK_U32 hor_stride = MPP_ALIGN(width, 16);
        RK_U32 ver_stride = MPP_ALIGN(height, 16);

        ret = mpp_buffer_group_get_internal(&data.frm_grp, MPP_BUFFER_TYPE_ION);
        if (ret) {
            printf("failed to get buffer group for input frame ret %d\n", ret);
            goto MPP_TEST_OUT;
        }

        ret = mpp_buffer_group_get_internal(&data.pkt_grp, MPP_BUFFER_TYPE_ION);
        if (ret) {
            printf("failed to get buffer group for output packet ret %d\n", ret);
            goto MPP_TEST_OUT;
        }

        ret = mpp_frame_init(&frame); /* output frame */
        if (MPP_OK != ret) {
            printf("mpp_frame_init failed\n");
            goto MPP_TEST_OUT;
        }

        /*
         * NOTE: For jpeg could have YUV420 and YUV422 the buffer should be
         * larger for output. And the buffer dimension should align to 16.
         * YUV420 buffer is 3/2 times of w*h.
         * YUV422 buffer is 2 times of w*h.
         * So create larger buffer with 2 times w*h.
         */
        ret = mpp_buffer_get(data.frm_grp, &frm_buf, hor_stride * ver_stride * 4);
        if (ret) {
            printf("failed to get buffer for input frame ret %d\n", ret);
            goto MPP_TEST_OUT;
        }

        // NOTE: for mjpeg decoding send the whole file
        if (type == MPP_VIDEO_CodingMJPEG) {
            packet_size = file_size;
        }

        ret = mpp_buffer_get(data.pkt_grp, &pkt_buf, packet_size);
        if (ret) {
            printf("failed to get buffer for input frame ret %d\n", ret);
            goto MPP_TEST_OUT;
        }
        mpp_packet_init_with_buffer(&packet, pkt_buf);
        buf = mpp_buffer_get_ptr(pkt_buf);

        mpp_frame_set_buffer(frame, frm_buf);
    }


    // decoder demo
    ret = mpp_create(&ctx, &mpi);

    if (MPP_OK != ret) {
        printf("mpp_create failed\n");
        goto MPP_TEST_OUT;
    }

    printf("%p mpi_dec_test decoder test start w %d h %d type %d\n",
           ctx, width, height, type);

    // NOTE: decoder split mode need to be set before init
    mpi_cmd = MPP_DEC_SET_PARSER_SPLIT_MODE;
    param = &need_split;
    ret = mpi->control(ctx, mpi_cmd, param);
    if (MPP_OK != ret) {
        printf("%p mpi->control failed\n", ctx);
        goto MPP_TEST_OUT;
    }

    // NOTE: timeout value please refer to MppPollType definition
    //  0   - non-block call (default)
    // -1   - block call
    // +val - timeout value in ms
    if (timeout) {
        param = &timeout;
        ret = mpi->control(ctx, MPP_SET_OUTPUT_TIMEOUT, param);
        if (MPP_OK != ret) {
            printf("%p failed to set output timeout %d ret %d\n",
                   ctx, timeout, ret);
            goto MPP_TEST_OUT;
        }
    }

    ret = mpp_init(ctx, MPP_CTX_DEC, type);
    if (MPP_OK != ret) {
        printf("%p mpp_init failed\n", ctx);
        goto MPP_TEST_OUT;
    }

    data.ctx            = ctx;
    data.mpi            = mpi;
    data.eos            = 0;
    data.buf            = buf;
    data.packet         = packet;
    data.packet_size    = packet_size;
    data.frame          = frame;
    data.frame_count    = 0;
    data.frame_num      = cmd->frame_num;

    if (cmd->simple) {
        while (!data.eos) {
            decode_simple(&data);
        }
    } else {
        /* NOTE: change output format before jpeg decoding */
        if (cmd->format < MPP_FMT_BUTT)
            ret = mpi->control(ctx, MPP_DEC_SET_OUTPUT_FORMAT, &cmd->format);

        while (!data.eos) {
            decode_advanced(&data);
        }
    }

    cmd->max_usage = data.max_usage;
    {
        MppDecQueryCfg query;

        memset(&query, 0, sizeof(query));
        query.query_flag = MPP_DEC_QUERY_ALL;
        ret = mpi->control(ctx, MPP_DEC_QUERY, &query);
        if (ret) {
            printf("%p mpi->control query failed\n", ctx);
            goto MPP_TEST_OUT;
        }

        /*
         * NOTE:
         * 1. Output frame count included info change frame and empty eos frame.
         * 2. Hardware run count is real decoded frame count.
         */
        printf("%p input %d pkt output %d frm decode %d frames\n", ctx,
               query.dec_in_pkt_cnt, query.dec_out_frm_cnt, query.dec_hw_run_cnt);
    }

    ret = mpi->reset(ctx);
    if (MPP_OK != ret) {
        printf("%p mpi->reset failed\n", ctx);
        goto MPP_TEST_OUT;
    }

MPP_TEST_OUT:
    if (packet) {
        mpp_packet_deinit(&packet);
        packet = NULL;
    }

    if (frame) {
        mpp_frame_deinit(&frame);
        frame = NULL;
    }

    if (ctx) {
        mpp_destroy(ctx);
        ctx = NULL;
    }

    if (cmd->simple) {
        if (buf) {
            mpp_free(buf);
            buf = NULL;
        }
    } else {
        if (pkt_buf) {
            mpp_buffer_put(pkt_buf);
            pkt_buf = NULL;
        }

        if (frm_buf) {
            mpp_buffer_put(frm_buf);
            frm_buf = NULL;
        }
    }

    if (data.pkt_grp) {
        mpp_buffer_group_put(data.pkt_grp);
        data.pkt_grp = NULL;
    }

    if (data.frm_grp) {
        mpp_buffer_group_put(data.frm_grp);
        data.frm_grp = NULL;
    }

    if (data.fp_output) {
        fclose(data.fp_output);
        data.fp_output = NULL;
    }

    if (data.fp_input) {
        fclose(data.fp_input);
        data.fp_input = NULL;
    }

    return ret;
}

static void mpi_dec_test_help()
{
    printf("usage: mpi_dec_test [options]\n");
    show_options(mpi_dec_cmd);
    mpp_show_support_format();
}

static RK_S32 mpi_dec_test_parse_options(int argc, char **argv, MpiDecTestCmd* cmd)
{
    const char *opt;
    const char *next;
    RK_S32 optindex = 1;
    RK_S32 handleoptions = 1;
    RK_S32 err = MPP_NOK;

    if ((argc < 2) || (cmd == NULL)) {
        err = 1;
        return err;
    }

    /* parse options */
    while (optindex < argc) {
        opt  = (const char*)argv[optindex++];
        next = (const char*)argv[optindex];

        if (handleoptions && opt[0] == '-' && opt[1] != '\0') {
            if (opt[1] == '-') {
                if (opt[2] != '\0') {
                    opt++;
                } else {
                    handleoptions = 0;
                    continue;
                }
            }

            opt++;

            switch (*opt) {
            case 'i':
                if (next) {
                    strncpy(cmd->file_input, next, MAX_FILE_NAME_LENGTH - 1);
                    cmd->have_input = 1;
                    name_to_coding_type(cmd->file_input, &cmd->type);
                } else {
                    printf("input file is invalid\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'o':
                if (next) {
                    strncpy(cmd->file_output, next, MAX_FILE_NAME_LENGTH - 1);
                    cmd->have_output = 1;
                } else {
                    printf("output file is invalid\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'c':
                if (next) {
                    strncpy(cmd->file_config, next, MAX_FILE_NAME_LENGTH - 1);
                    cmd->have_config = 1;

                    // enlarge packet buffer size for large input stream case
                    cmd->pkt_size = SZ_1M;
                } else {
                    printf("output file is invalid\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'd':
                if (next) {
                    cmd->debug = atoi(next);;
                } else {
                    printf("invalid debug flag\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'w':
                if (next) {
                    cmd->width = atoi(next);
                } else {
                    printf("invalid input width\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'h':
                if ((*(opt + 1) != '\0') && !strncmp(opt, "help", 4)) {
                    mpi_dec_test_help();
                    err = 1;
                    goto PARSE_OPINIONS_OUT;
                } else if (next) {
                    cmd->height = atoi(next);
                } else {
                    printf("input height is invalid\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 't':
                if (next) {
                    cmd->type = (MppCodingType)atoi(next);
                    err = mpp_check_support_format(MPP_CTX_DEC, cmd->type);
                }

                if (!next || err) {
                    printf("invalid input coding type\n");
                    printf("error\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'f':
                if (next) {
                    cmd->format = (MppFrameFormat)atoi(next);
                    printf("cmd->format %d\n", cmd->format);
                }

                if (!next || err) {
                    printf("invalid input coding type\n");
                    printf("error\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'x':
                if (next) {
                    cmd->timeout = atoi(next);
                }

                if (!next || cmd->timeout < 0) {
                    printf("invalid output timeout interval\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            case 'n':
                if (next) {
                    cmd->frame_num = atoi(next);
                    if (cmd->frame_num < 0)
                        printf("infinite loop decoding mode\n");
                } else {
                    printf("invalid frame number\n");
                    goto PARSE_OPINIONS_OUT;
                }
                break;
            default:
                printf("skip invalid opt %c\n", *opt);
                break;
            }

            optindex++;
        }
    }

    err = 0;

PARSE_OPINIONS_OUT:
    return err;
}

static void mpi_dec_test_show_options(MpiDecTestCmd* cmd)
{
    printf("cmd parse result:\n");
    printf("input  file name: %s\n", cmd->file_input);
    printf("output file name: %s\n", cmd->file_output);
    printf("config file name: %s\n", cmd->file_config);
    printf("width      : %4d\n", cmd->width);
    printf("height     : %4d\n", cmd->height);
    printf("type       : %d\n", cmd->type);
    printf("debug flag : %x\n", cmd->debug);
    printf("max frames : %d\n", cmd->frame_num);
}

//int main(int argc, char **argv)
int mpp_jpg2yuv()
{
    RK_S32 ret = 0;
    MpiDecTestCmd  cmd_ctx;
    MpiDecTestCmd* cmd = &cmd_ctx;

    memset((void*)cmd, 0, sizeof(*cmd));
    cmd->format = MPP_FMT_BUTT;
    cmd->pkt_size = MPI_DEC_STREAM_SIZE;

#if 0
    strncpy(cmd->file_input, "test.jpeg", MAX_FILE_NAME_LENGTH - 1);
    cmd->have_input = 1;
    name_to_coding_type(cmd->file_input, &cmd->type);

    strncpy(cmd->file_output, "test.nv12", MAX_FILE_NAME_LENGTH - 1);
    cmd->have_output = 1;

    cmd->width = 640;
    cmd->height = 360;
    //-i test.jpeg -o test.nv12 -w 640 -h 360
#endif
    char *argv[9] = { "0", "-i", "test.jpeg", "-o", "test.nv12",
                      "-w", "640", "-h", "360"
                    };
    int argc = 9;
    // parse the cmd option
#if 1
    ret = mpi_dec_test_parse_options(argc, argv, cmd);
    if (ret) {
        if (ret < 0) {
            printf("mpi_dec_test_parse_options: input parameter invalid\n");
        }

        mpi_dec_test_help();
        return ret;
    }
#endif
    mpi_dec_test_show_options(cmd);

    mpp_env_set_u32("mpi_debug", cmd->debug);

    cmd->simple = (cmd->type != MPP_VIDEO_CodingMJPEG) ? (1) : (0);

    ret = mpi_dec_test_decode(cmd);
    if (MPP_OK == ret)
        printf("test success max memory %.2f MB\n", cmd->max_usage / (float)(1 << 20));
    else
        printf("test failed ret %d\n", ret);

    mpp_env_set_u32("mpi_debug", 0x0);
    return ret;
}

