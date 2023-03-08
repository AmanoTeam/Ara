#include <stdlib.h>

#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>

#define MAX_STREAMS 30

#define __avformat_close_inputs__ __attribute__((__cleanup__(close_inputs_cleanup)))
#define __avformat_free_context__ __attribute__((__cleanup__(output_context_cleanup)))

static void close_inputs_cleanup(AVFormatContext*** inputs_format_context) {
	
	if (*inputs_format_context == NULL) {
		return;
	}
	
	for (size_t index = 0; index < MAX_STREAMS; index++) {
		AVFormatContext** input_format_context = &(*inputs_format_context)[index];
		
		if (input_format_context == NULL) {
			break;
		}
		
		avformat_close_input(input_format_context);
	}
	
	free(*inputs_format_context);
	
}

static void output_context_cleanup(AVFormatContext** output_format_context) {
	
	if (*output_format_context == NULL) {
		return;
	}
	
	if (!((*output_format_context)->oformat->flags & AVFMT_NOFILE)) {
		avio_closep(&(*output_format_context)->pb);
	}
	
	avformat_free_context(*output_format_context);
	
}

int ffmpeg_copy_streams(const char* const* const sources, const char* const destination) {
	
	av_log_set_level(AV_LOG_QUIET);
	
	int code = 0;
	
	AVDictionary* options = NULL;
	av_dict_set(&options, "allowed_extensions", "ALL", 0);
	
	int streams_index[MAX_STREAMS];
	int stream_index = 0;
	
	AVFormatContext** inputs_context __avformat_close_inputs__ = malloc(sizeof(AVFormatContext*) * MAX_STREAMS);
	
	if (inputs_context == NULL) {
		return AVERROR_UNKNOWN;
	}
	
	for (size_t index = 0; index < MAX_STREAMS; index++) {
		inputs_context[index] = NULL;
	}
	
	AVFormatContext* output_format_context __avformat_free_context__ = NULL;
	code = avformat_alloc_output_context2(&output_format_context, NULL, NULL, destination);
	
	if (code < 0) {
		return code;
	}
	
	for (size_t source_index = 0;; source_index++) {
		const char* const source = sources[source_index];
		
		if (source == NULL) {
			break;
		}
		
		AVFormatContext* input_format_context = NULL;
		code = avformat_open_input(&input_format_context, source, NULL, &options);
		
		if (code != 0) {
			return code;
		}
		
		inputs_context[source_index] = input_format_context;
		
		code = avformat_find_stream_info(input_format_context, NULL);
		
		if (code < 0) {
			return code;
		}
		
		for (size_t index = 0; index < input_format_context->nb_streams; index++) {
			AVStream* const input_stream = input_format_context->streams[index];
			
			const int output_stream_index = index + source_index;
			
			if (!(input_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO || input_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)) {
				streams_index[output_stream_index] = -1;
				continue;
			}
			
			streams_index[output_stream_index] = stream_index++;
			
			AVStream* const output_stream = avformat_new_stream(output_format_context, NULL);
			
			if (output_stream == NULL) {
				return AVERROR_UNKNOWN;
			}
			
			const int code = avcodec_parameters_copy(output_stream->codecpar, input_stream->codecpar);
			
			if (code < 0) {
				return code;
			}
			
			output_stream->codecpar->codec_tag = 0;
		}
	}
	
	if (!(output_format_context->oformat->flags & AVFMT_NOFILE)) {
		const int code = avio_open(&output_format_context->pb, destination, AVIO_FLAG_WRITE);
		
		if (code < 0) {
			return code;
		}
	}
	
	code = avformat_write_header(output_format_context, NULL);
	
	if (code < 0) {
		return code;
	}
	
	AVPacket packet = {0};
	
	for (size_t source_index = 0;; source_index++) {
		const char* const source = sources[source_index];
		
		if (source == NULL) {
			break;
		}
		
		AVFormatContext* const input_format_context = inputs_context[source_index];
		
		while (1) {
			code = av_read_frame(input_format_context, &packet);
			
			if (code == AVERROR_EOF) {
				break;
			}
			
			if (code < 0) {
				return code;
			}
			
			const int input_stream_index = packet.stream_index;
			const int output_stream_index = input_stream_index + source_index;
			
			const int should_discard = (packet.pts == AV_NOPTS_VALUE || output_stream_index >= MAX_STREAMS || streams_index[output_stream_index] == -1);
			
			if (should_discard) {
				av_packet_unref(&packet);
				continue;
			}
			
			const AVStream* const input_stream = input_format_context->streams[input_stream_index];
			const AVStream* const output_stream = output_format_context->streams[output_stream_index];
			
			packet.pts = av_rescale_q_rnd(packet.pts, input_stream->time_base, output_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
			packet.dts = av_rescale_q_rnd(packet.dts, input_stream->time_base, output_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
			packet.duration = av_rescale_q(packet.duration, input_stream->time_base, output_stream->time_base);
			packet.pos = -1;
			
			packet.stream_index = output_stream_index;
			code = av_interleaved_write_frame(output_format_context, &packet);
			
			av_packet_unref(&packet);
			
			if (code != 0) {
				return code;
			}
		}
	}
	
	code = av_write_trailer(output_format_context);
	
	if (code != 0) {
		return code;
	}
	
	return 0;
	
}
