/************************************************************************************
	* Author:	Oso, Oluwafemi Ebenezer
	* Date:		5th of June, 2014
	* Filename:	WaveFileHeader.h

	A struct for the PCM wave header. Can be used to write the header of a wave file
**************************************************************************************/
#include "WaveFileHeader.h"

WaveFileHeader::WaveFileHeader()
{
}

//Build wave header
WaveFileHeader::WaveFileHeader(unsigned long SamplingRate, unsigned short BitsPerSample, unsigned short Channels, unsigned long DataSize)
{

	// Set Riff-Chunk
	memcpy(Riff_ID, "RIFF", 4);
	Riff_Size = DataSize + 44;
	memcpy(Riff_Type, "WAVE", 4);

	// Set Fmt-Chunk
	memcpy(Fmt_ID, "fmt ", 4);
	Fmt_Length = 16;
	Fmt_Format = 1;
	Fmt_Channels = Channels;
	Fmt_SamplingRate = SamplingRate;
	Fmt_BlockAlign = Channels*BitsPerSample/8;
	Fmt_DataRate = Channels*BitsPerSample*SamplingRate/8;
	Fmt_BitsPerSample = BitsPerSample;

	// Set Data-Chunk
	memcpy(Data_ID, "data", 4);
	Data_DataSize = DataSize;
}