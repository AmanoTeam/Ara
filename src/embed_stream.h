enum EmbedStreamType {
	EMBED_STREAM_VIMEO
};

struct EmbedStream {
	enum EmbedStreamType type;
	char* uri;
};

int embed_stream_find(const char* const content, struct EmbedStream* const info);
void embed_stream_free(struct EmbedStream* const obj);