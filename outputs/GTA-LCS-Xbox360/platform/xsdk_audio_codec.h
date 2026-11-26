// Portable VAG decoder adapted from the pinned reLCS PowerPC reference.
// No PS3 platform code. Stereo .VB: 0x2000-byte interleave, 32000 Hz.
class Decoder
{
public:
	virtual ~Decoder() {}
	virtual bool IsOpened() = 0;
	virtual uint32 GetRate() = 0;
	virtual uint32 GetLengthMS() = 0;
	virtual void Seek(uint32 ms) = 0;
	// interleaved stereo frames; 0 at the end
	virtual uint32 Decode(int16 *out, uint32 maxFrames) = 0;
};

static inline uint16 rd16(const uint8 *p) { return p[0] | p[1]<<8; }
static inline uint32 rd32(const uint8 *p) { return p[0] | p[1]<<8 | p[2]<<16 | (uint32)p[3]<<24; }

#define VB_BLOCK_SIZE	0x2000
#define VAG_LINE_SIZE	16
#define VAG_LINE_SAMPLES	28
#define VB_BLOCK_LINES	(VB_BLOCK_SIZE / VAG_LINE_SIZE)
#define VB_BLOCK_SAMPLES	(VB_BLOCK_LINES * VAG_LINE_SAMPLES)

class VbDecoder : public Decoder
{
	enum { CHANNELS = 2 };
	FILE *f;
	uint32 rate;
	uint32 numBlocks;
	uint32 block, line;	// the next line to decode
	bool loaded;		// buf holds block
	uint32 filePos;
	uint8 buf[CHANNELS][VB_BLOCK_SIZE];
	int32 hist[CHANNELS][2];

	static void DecodeLine(const uint8 *in, int16 *out, int32 *h)
	{
		static const int32 f0[5] = { 0, 60, 115, 98, 122 };
		static const int32 f1[5] = { 0, 0, -52, -55, -60 };
		int32 predict = in[0] >> 4;
		int32 shift = in[0] & 0xF;
		int32 flags = in[1];
		if(predict > 4) predict = 0;
		if(shift > 12) shift = 12;
		if(flags == 7){
			// end marker: a silent line
			for(int i = 0; i < VAG_LINE_SAMPLES; i++)
				out[i] = 0;
			return;
		}
		for(int i = 0; i < VAG_LINE_SAMPLES; i++){
			int32 d = in[2 + i/2];
			int32 nib = (i & 1) ? d >> 4 : d & 0xF;
			int32 s = (int16)(nib << 12) >> shift;
			s += (h[0]*f0[predict] + h[1]*f1[predict] + 32) >> 6;
			if(s > 32767) s = 32767;
			if(s < -32768) s = -32768;
			h[1] = h[0];
			h[0] = s;
			out[i] = (int16)s;
		}
	}

public:
	VbDecoder(const char *path, uint32 sampleRate) : f(nil), rate(sampleRate), numBlocks(0), block(0), line(0),
		loaded(false), filePos(0)
	{
		memset(hist, 0, sizeof(hist));
		f = fcaseopen(path, "rb");
		if(f == nil)
			return;
		fseek(f, 0, SEEK_END);
		uint32 size = ftell(f);
		fseek(f, 0, SEEK_SET);
		numBlocks = size / (CHANNELS * VB_BLOCK_SIZE);
	}
	~VbDecoder()
	{
		if(f) fclose(f);
	}
	bool IsOpened() { return f != nil && numBlocks != 0; }
	uint32 GetRate() { return rate; }
	uint32 GetLengthMS() { return (uint32)((uint64)numBlocks * VB_BLOCK_SAMPLES * 1000 / rate); }
	void Seek(uint32 ms)
	{
		uint32 sample = (uint32)((uint64)ms * rate / 1000);
		uint32 b = sample / VB_BLOCK_SAMPLES;
		if(b >= numBlocks){
			b = 0;
			sample = 0;
		}
		if(b != block)
			loaded = false;
		block = b;
		line = (sample - b * VB_BLOCK_SAMPLES) / VAG_LINE_SAMPLES;
		memset(hist, 0, sizeof(hist));
	}
	uint32 Decode(int16 *out, uint32 maxFrames)
	{
		int16 pcm[CHANNELS][VAG_LINE_SAMPLES];
		uint32 n = 0;
		while(n + VAG_LINE_SAMPLES <= maxFrames && block < numBlocks){
			if(!loaded){
				uint32 at = block * CHANNELS * VB_BLOCK_SIZE;
				if(at != filePos && fseek(f, at, SEEK_SET) != 0)
					break;
				filePos = at;
				if(fread(buf, 1, sizeof(buf), f) != sizeof(buf)){
					filePos = UINT32_MAX;
					break;
				}
				filePos += sizeof(buf);
				loaded = true;
			}
			for(int c = 0; c < CHANNELS; c++)
				DecodeLine(&buf[c][line * VAG_LINE_SIZE], pcm[c], hist[c]);
			for(int i = 0; i < VAG_LINE_SAMPLES; i++){
				out[n*2] = pcm[0][i];
				out[n*2+1] = pcm[1][i];
				n++;
			}
			if(++line >= VB_BLOCK_LINES){
				line = 0;
				block++;
				loaded = false;
			}
		}
		return n;
	}
};
