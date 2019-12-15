/************************************************************************************
Function decode based on https://www.codeproject.com/Articles/824199/The-OGG-Wrapper-An-audio-converter
**************************************************************************************/

#define LIB_NAME "DecodeOgg"
#define MODULE_NAME "decodeogg"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <vorbis/vorbisenc.h>
#include <deque>
#include "WaveFileHeader.h"
#include <sstream>
#include <string>

static int decode(char* in_bytes, uint32_t size, char* &out_bytes);
static int read_to_buffer(char* buffer_out, int read_size, int size, char* bytes_in);
static int decodeogg(lua_State* L);

static uint32_t current_read_size = 0; //Need to read blocks. Global values is bad

static int decodeogg(lua_State* L)
{
    int top = lua_gettop(L);

	//copy buffer from res
    dmScript::LuaHBuffer *lua_buffer = dmScript::CheckBuffer(L, 1);
	dmBuffer::HBuffer buffer = lua_buffer->m_Buffer;
	if (!dmBuffer::IsBufferValid(buffer)) 
	{
		luaL_error(L, "Buffer is invalid");
	}
    uint32_t in_bytes_size;
	char* in_bytes;
	
	dmBuffer::Result r_get_in_bytes = dmBuffer::GetBytes(buffer, (void**)&in_bytes, &in_bytes_size);
	if (r_get_in_bytes != dmBuffer::RESULT_OK) 
	{
		luaL_error(L, "Buffer is invalid");
	}

	//Decoding
	//Warning! Function decode allocates memory by pointer out_bytes. This should be freed using delete[]
	char* out_bytes = nullptr;
	uint32_t out_size = decode(in_bytes, in_bytes_size, out_bytes);
	if(!out_size)
	{
		luaL_error(L, "Invalid ogg file");
	}
	dmBuffer::Destroy(buffer);
	//Create and fill new buffer
	const dmBuffer::StreamDeclaration streams_decl[] = 
	{
		{dmHashString64("byte"), dmBuffer::VALUE_TYPE_UINT8, 1},
	};

	dmBuffer::Result r_create = dmBuffer::Create(out_size, streams_decl, 1, &buffer);
	if (r_create == dmBuffer::RESULT_OK) 
	{
		char* wav_bytes;
		uint32_t wav_size;
		dmBuffer::Result r_get_wav_bytes = dmBuffer::GetBytes(buffer, (void**)&wav_bytes, &wav_size);
		if (r_get_wav_bytes == dmBuffer::RESULT_OK) 
		{
			for(size_t i = 0;i < wav_size;++i)
			{
				wav_bytes[i] = out_bytes[i];
			}
			lua_buffer->m_Buffer = buffer;
		}
		else 
		{
			luaL_error(L, "Invalid get wav bytes");
		}
	} 
	else 
	{
		luaL_error(L, "Invalid create new buffer");
	}
	if(out_size)
	{
		delete[] out_bytes;
	}
    // Assert that there is one item on the stack.
    assert(top == lua_gettop(L));

    // Return 1 item
    return 1;
}

static int decode(char* in_bytes, uint32_t size, char* &out_bytes)
{
	std::stringstream s_out;
	current_read_size = 0;
	if(!in_bytes)
	{
		printf("in_bytes is NULL! Aborting!");
		return 0;
	}

	uint32_t cumulative_read = 0;
	uint32_t ogg_total_size = size;


	ogg_int16_t convbuffer[4096]; /* take 8k out of the data segment, not the stack */
	uint32_t convsize=4096;

	uint32_t byteWritten = 0;
	ogg_sync_state   oy; /* sync and verify incoming physical bitstream */
	ogg_stream_state os; /* take physical pages, weld into a logical
	stream of packets */
	ogg_page         og; /* one Ogg bitstream page. Vorbis packets are inside */
	ogg_packet       op; /* one raw packet of data for decode */

	vorbis_info      vi; /* struct that stores all the static vorbis bitstream
	settings */

	vorbis_comment   vc; /* struct that stores all the bitstream user comments */
	vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
	vorbis_block     vb; /* local working space for packet->PCM decode */


	char *buffer;
	uint32_t  bytes;

	/********** Decode setup ************/
	ogg_sync_init(&oy); /* Now we can read pages */

	while(1)
	{ 
		/* we repeat if the bitstream is chained */
		uint32_t eos=0;
		uint32_t i;


		/* grab some data at the head of the stream. We want the first page
		(which is guaranteed to be small and only contain the Vorbis
			stream initial header) We need the first page to get the stream
			serialno. */

		/* submit a 4k block to libvorbis' Ogg layer */
		buffer=ogg_sync_buffer(&oy, 4096);
		bytes = read_to_buffer(buffer, 4096, size, in_bytes);
		ogg_sync_wrote(&oy, bytes);

		cumulative_read += bytes;

		/* Get the first page. */
		if(ogg_sync_pageout(&oy,&og)!= 1)
		{
			/* have we simply run out of data?  If so, we're done. */
			if(bytes<4096)
			break;

			/* error case.  Must not be Vorbis data */
			printf("Input does not appear to be an Ogg bitstream! Aborting!");
			return 0;
		}

		/* Get the serial number and set up the rest of decode. */
		/* serialno first; use it to set up a logical stream */
		ogg_stream_init(&os, ogg_page_serialno(&og));

		/* extract the initial header from the first page and verify that the
		Ogg bitstream is in fact Vorbis data */

		/* I handle the initial header first instead of just having the code
		read all three Vorbis headers at once because reading the initial
		header is an easy way to identify a Vorbis bitstream and it's
		useful to see that functionality seperated out. */


		vorbis_info_init(&vi);
		vorbis_comment_init(&vc);

		if(ogg_stream_pagein(&os,&og)<0)
		{ 
			/* error; stream version mismatch perhaps */
			printf("Error reading first page of Ogg bitstream data! Aborting!");
			return 0;
		}


		if(ogg_stream_packetout(&os,&op)!=1)
		{ 
			/* no page? must not be vorbis */
			printf("Error reading initial header packet! Aborting!");
			return 0;
		}

		if(vorbis_synthesis_headerin(&vi,&vc,&op)<0)
		{ 
			/* error case; not a vorbis header */
			printf("This Ogg bitstream does not contain Vorbis audio data! Aborting!");
			return 0;
		}


		/* At this point, we're sure we're Vorbis. We've set up the logical
		(Ogg) bitstream decoder. Get the comment and codebook headers and
		set up the Vorbis decoder */


		/* The next two packets in order are the comment and codebook headers.
		They're likely large and may span multiple pages. Thus we read
		and submit data until we get our two packets, watching that no
		pages are missing. If a page is missing, error out; losing a
		header page is the only place where missing data is fatal. */

		i=0;
		while(i<2)
		{
			while(i<2)
			{
				int result=ogg_sync_pageout(&oy,&og);
				if(result==0)break; /* Need more data */

				/* Don't complain about missing or corrupt data yet. We'll catch it at the packet output phase */
				if(result==1)
				{
					ogg_stream_pagein(&os,&og); /* we can ignore any errors here
					as they'll also become apparent
					at packetout */
					while(i<2)
					{

						result=ogg_stream_packetout(&os,&op);
						if(result==0)break;
						if(result<0)
						{
							/* Uh oh; data at some point was corrupted or missing! We can't tolerate that in a header.  Die. */
							printf("Corrupt secondary header! Aborting!");
							return 0;
						}
						result=vorbis_synthesis_headerin(&vi,&vc,&op);

						if(result<0)
						{
							printf("Corrupt secondary header! Aborting!");
							return 0;
						}
						i++;
					}
				}
			}

			/* no harm in not checking before adding more */
			buffer=ogg_sync_buffer(&oy,4096);
			bytes = read_to_buffer(buffer, 4096, size, in_bytes);

			cumulative_read += bytes;

			if(bytes==0 && i<2)
			{
				printf("End of file before finding all Vorbis headers! Aborting!");
				return 0;
			}
			ogg_sync_wrote(&oy,bytes);
		}

		convsize=4096/vi.channels;


		/* OK, got and parsed all three headers. Initialize the Vorbis packet->PCM decoder. */
		if(vorbis_synthesis_init(&vd,&vi)==0)
		{ /* central decode state */
			vorbis_block_init(&vd, &vb); /* local state for most of the decode so multiple block decodes can 
			proceed in parallel. We could init
			multiple vorbis_block structures for vd here */

			/* The rest is just a straight decode loop until end of stream */
			while(!eos)
			{
				while(!eos)
				{
					int result=ogg_sync_pageout(&oy,&og);
					if(result==0)
					break; /* need more data */

					if(result<0)
					{ 
						/* missing or corrupt data at this page position */
						printf("Corrupt or missing data in bitstream! Continuing...");
					}
					else
					{
						ogg_stream_pagein(&os,&og); /* can safely ignore errors at this point */

						while(1)
						{
							result=ogg_stream_packetout(&os,&op);
							if(result==0)
							break; /* need more data */

							if(result<0)
							{ /* missing or corrupt data at this page position */
								/* no reason to complain; already complained above */
							}
							else
							{
								/* we have a packet.  Decode it */
								float **pcm;
								int samples;
								if(vorbis_synthesis(&vb,&op)==0) /* test for success! */
								vorbis_synthesis_blockin(&vd,&vb);

								/* **pcm is a multichannel float vector.  In stereo, for
								example, pcm[0] is left, and pcm[1] is right.  samples is
								the size of each channel.  Convert the float values
								(-1.<=range<=1.) to whatever PCM format and write it out */


								while((samples=vorbis_synthesis_pcmout(&vd,&pcm))>0)
								{
									int j;
									int clipflag=0;
									int bout=(samples<convsize?samples:convsize);

									/* convert floats to 16 bit signed ints (host order) and interleave */
									for(i=0;i<vi.channels;i++)
									{
										ogg_int16_t *ptr=convbuffer+i;
										float  *mono=pcm[i];
										for(j=0;j<bout;j++)
										{
											int val=floor(mono[j]*32767.f+.5f);

											/* might as well guard against clipping */
											if(val>32767)
											{
												val=32767;
												clipflag=1;
											}
											if(val<-32768)
											{
												val=-32768;
												clipflag=1;
											}
											*ptr=val;
											ptr+=vi.channels; 
										}
									}
									s_out.write((char*)convbuffer, 2*vi.channels*bout);
									int written = bout;
									byteWritten += (written*2*vi.channels);

									vorbis_synthesis_read(&vd, bout); /* tell libvorbis how many samples we actually consumed */
								}            
							}
						}
						if(ogg_page_eos(&og))
						eos=1;
					}
				}

				if(!eos)
				{
					buffer=ogg_sync_buffer(&oy,4096);
					bytes = read_to_buffer(buffer, 4096, size, in_bytes);

					cumulative_read += bytes;
					ogg_sync_wrote(&oy,bytes);
					if(bytes==0)
					eos=1;
				}
			}

			/* ogg_page and ogg_packet structs always point to storage in libvorbis.  They're never freed or manipulated directly */
			vorbis_block_clear(&vb);
			vorbis_dsp_clear(&vd);

			//Seek back and write the WAV header
			WaveFileHeader wfh = WaveFileHeader(vi.rate, 16, vi.channels, byteWritten);
			s_out.seekp(0, std::ios_base::beg);
			s_out.write((char*)&wfh, sizeof(wfh));
		}
		else
		{
			printf("Error: Corrupt header during playback initialization! Aborting!");
			return 0;
		}

		/* clean up this logical bitstream; before exit we see if we're followed by another [chained] */
		ogg_stream_clear(&os);
		vorbis_comment_clear(&vc);  
		vorbis_info_clear(&vi);  /* must be called last */  
	}
	/* OK, clean up the framer */ 
	ogg_sync_clear(&oy);
			
	s_out.seekp(0, std::ios::end);
	std::stringstream::pos_type offset = s_out.tellp();
	out_bytes = new char[offset];
	s_out.seekp(0, std::ios_base::beg);
	s_out.read(out_bytes,offset);
	return offset;
}

//Implemented to read from block buffer
static int read_to_buffer(char* buffer_out, int read_size, int size, char* bytes_in)
{
	int k = 0;
	for(k = 0;k < read_size && current_read_size + k < size;++k)
	{
		buffer_out[k] = bytes_in[current_read_size + k];
	}
	current_read_size = current_read_size + k;
	//printf("Readed size is %d", k);
	return k;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"decodeogg", decodeogg},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeDecodeOgg(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeDecodeOgg(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    printf("Registered %s Extension\n", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeDecodeOgg(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeDecodeOgg(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(DecodeOgg, LIB_NAME, AppInitializeDecodeOgg, AppFinalizeDecodeOgg, InitializeDecodeOgg, 0, 0, FinalizeDecodeOgg)
