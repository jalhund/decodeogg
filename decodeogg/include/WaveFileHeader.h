/************************************************************************************
	* Author:	Oso, Oluwafemi Ebenezer
	* Date:		5th of June, 2014
	* Filename:	WaveFileHeader.h

	A struct for the PCM wave header. Can be used to write the header of a wave file
**************************************************************************************/
#include <stdio.h>
#include <string.h>
//struct for wave header
struct WaveFileHeader
{
	WaveFileHeader();
	WaveFileHeader(unsigned long SamplingRate, unsigned short BitsPerSample, unsigned short Channels, unsigned long DataSize);

	unsigned char  Riff_ID[4];			//'RIFF'
	unsigned int  Riff_Size;			// DataSize + 44
	unsigned char  Riff_Type[4];		// 'WAVE'

	unsigned char  Fmt_ID[4];			// 'fmt '
	unsigned int  Fmt_Length;			// 16
	unsigned short Fmt_Format;			// 1 (=MS PCM)
	unsigned short Fmt_Channels;		// 1 = mono, 2 = stereo
	unsigned int  Fmt_SamplingRate;	// (e.g 44100)
	unsigned int  Fmt_DataRate;		// SamplingRate * BlockAlign
	unsigned short Fmt_BlockAlign;		// Channels * BitsPerSample / 8
	unsigned short Fmt_BitsPerSample;	// 8 or 16

	unsigned char  Data_ID[4];			// 'data'
	unsigned int  Data_DataSize;		// Size of the following data
};