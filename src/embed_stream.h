enum EmbedStreamType {
	EMBED_STREAM_VIMEO,
	EMBED_STREAM_YOUTUBE
};

struct EmbedStream {
	enum EmbedStreamType type;
	char* uri;
};

int embed_stream_find(const char* const content, struct EmbedStream* const info);
void embed_stream_free(struct EmbedStream* const obj);