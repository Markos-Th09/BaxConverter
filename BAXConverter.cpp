// BAXConverter.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <cstdint>
#include <stdexcept>
#include "opencv2/opencv.hpp"

//#include "raw.h"
#include "lz4.h"
#include "lz4hc.h"

#ifndef _WIN32
#include <strings.h>
#define _strcmpi strcasecmp
#endif

class Frame {
public:
	char* compressed = NULL;
	size_t compressed_size = 0;
	uint16_t* uncompressed=NULL;
	size_t uncompressed_size = 0;
};
uint32_t BGR2RGB(uchar* bgr)
{
	return (bgr[2] << 16) | (bgr[1] << 8) | (bgr[0]);
}
uint16_t RGB8882RGB565(uint32_t rgb)
{
	uint16_t r = (rgb & 0b111110000000000000000000)>>8;
	uint16_t g = (rgb & 0b1111110000000000) >> 5;
	uint16_t b = (rgb & 0b11111000) >> 3;
	return r | g | b;
}
uint16_t BGR2RGB565(uchar* bgr)
{
	return RGB8882RGB565(BGR2RGB(bgr));
}
uint32_t RGB5652RGB888(uint16_t rgb565)
{
	uint8_t r = ((rgb565 >> 8) & 0b11111000) | ((rgb565 >> 13) & 0b111);
	uint8_t g = ((rgb565 >> 3) & 0b11111100) | ((rgb565>>9)&0b11);
	uint8_t b = (rgb565 << 3)|((rgb565>>2) | 0b111);
	//std::cout << std::hex << "RGB565: " << std::setw(2) << std::setfill('0') << rgb565 << "\nRGB888: " << r << g << b << std::endl;
	return (r << 16) | (g << 8) | (b);
}
struct BAX_HEADER {
	char magic[3];
	char version;
	uint32_t flags;
	uint32_t frame_count;
	uint32_t frame_rate;
	uint16_t bgcolor;
	uint16_t reserved;
	uint32_t reserved2;
	uint32_t col_offset;
	uint32_t frame_width;
	char author[32];
	char info[192];
};

bool usetop = true;
bool usebottom = false;
uint16_t bgColor = 0; // black
char author[32] = "BAXConverter";
char info[192] = "Created by BAXConverter";
char video[256] = "bax.mp4";
char* baseImage; // will be set to max size with bgColor for memcpy commands
int maxSeconds = 10;
void Portraitize(Frame &o)
{
	Frame f;
	f.uncompressed = (uint16_t*)malloc(720 * 240 * 2);
	f.uncompressed_size = 720 * 240 * 2;
	memcpy(f.uncompressed, baseImage, f.uncompressed_size);
	for (int x = 0; x < 320; x++)
	{
		for (int y = 0; y < 240; y++)
		{
			size_t src1 = x * (uint32_t)480 + y;
			size_t src2 = x * (uint32_t)480 + y + 240;
			size_t dest1 = (x+40) * (uint32_t)240 + y;
			size_t dest2 = (x+400) * (uint32_t)240 + y;
			//uchar* imColor = im.ptr(y, x);
			//uint16_t rgb565 = BGR2RGB565(imColor);
			f.uncompressed[dest1] = o.uncompressed[src2];
			f.uncompressed[dest2] = o.uncompressed[src1];
			//frame.uncompressed[loc] = rgb565;
		}
	}
	free(o.uncompressed); // grab reference for release;
	o.uncompressed = f.uncompressed; // reference the new data
	f.uncompressed = NULL; // dereference
	o.uncompressed_size = f.uncompressed_size;

}

std::vector<std::pair<char, char*>> getArgs(char** argv, int argc)
{
	std::vector<std::pair<char, char*>> ret;
	if (argc == 2)
	{
		return ret;
	}
	for (int x = 1; x < argc; x++)
	{
		std::pair<char, char*> p;
		if (argv[x][0] == '-' && argc >= x+2)
		{
			p = std::make_pair(argv[x][1], argv[x+1]);
			ret.push_back(p);
			x++;
		}
		else {
			p = std::make_pair('0', argv[x]);
			ret.push_back(p);
		}
	}
	return ret;
}

int main(int argc, char** argv)
{
	// the following line is to make opencv stfu
	cv::redirectError([](int s, const char* fn, const char* em, const char* fln, int l, void* u) {return 0; });

	//Defaults
	std::vector<std::pair<char, char*>> Args;
	//gather args
	if (argc == 1)
	{
		std::cout << "BaxConverter <video>" << std::endl;
		std::cout << "BaxConverter -i <video> [-s bottom|top|both] [-b background_color]" << std::endl;
		std::cout << "ex: BaxConverter NH.mp4" << std::endl;
		std::cout << "ex: BaxConverter -i NH.mp4 -s bottom -b FF00FF" << std::endl;
		return 0;
	}
	if (argc == 2)
		strncpy(video, argv[1], 256);
	else
		Args = getArgs(argv, argc);

	for (int x = 0; x < Args.size(); x++)
	{
		std::stringstream ss;
		ss << Args[x].second;

		switch (Args[x].first) {
		case 'i':
			ss >> video;
			break;
		case 's':
			if (_strcmpi(Args[x].second, "both") == 0)
			{
				usetop = true;
				usebottom = true;
				break;
			}
			if (_strcmpi(Args[x].second, "top") == 0)
			{
				usetop = true;
				usebottom = false;
			}
			if (_strcmpi(Args[x].second, "bottom") == 0)
			{
				usetop = false;
				usebottom = true;
			}
			break;
		case 'b':
		{
			std::string s;

			ss >> s;
			if (s.length() == 6) // hex
			{
				ss.clear();
				ss << std::hex << s;
				uint32_t bg;
				ss >> bg;

				//std::cout << ((bg & 0xFF0000) >> 19) << ":" << ((bg & 0xFF00) >> 10) << ":" << ((bg & 0b1111100) >> 2) << std::endl;
				bgColor = RGB8882RGB565(bg);
			}
			else if (s.length() <= 4)
			{
				ss >> bgColor;
			}
		}
			break;
		case '0':
			ss >> video;
			break;
		case 't':
			ss >> maxSeconds;
			break;
		}
	}
	struct BAX_HEADER baxh = { "",1,0,0,0,0,0,0,0,0,"\0","\0" };
	strncpy(baxh.magic,"BAX",3);
	baxh.version = 1;
	baxh.reserved = 0;
	baxh.reserved2 = 0;
	baxh.bgcolor = bgColor;

	std::cout << "Opening video: " << video << std::endl;

	cv::VideoCapture cap(video);
	if (cap.isOpened()) {
		std::cout << "Top Screen:\t\t" << ((usetop)? "True":"False") << "\nBottom Screen:\t\t" << ((usebottom)?"True":"False") << std::endl;
		uint32_t rgb888 = RGB5652RGB888(bgColor);
		std::cout << "Background Color:\t" << std::setw(6) << std::setfill('0')<< std::hex << rgb888 << std::dec << std::endl;
		baseImage = (char*)malloc(720 * 240 * 2);
		for (int i = 0; i < 720 * 240; i++)
			reinterpret_cast<uint16_t*>(baseImage)[i] = bgColor;
		double dFrames = cap.get(cv::CAP_PROP_FRAME_COUNT);
		double dFPS = cap.get(cv::CAP_PROP_FPS);
		size_t maxFrames = maxSeconds * dFPS;
		baxh.frame_rate = dFPS;

		if (dFrames > maxFrames)
		{
			std::cout << "Video is longer than" << maxSeconds <<" seconds, limiting to " << maxSeconds <<" seconds" << std::endl;
		}
		std::cout << "Framerate:\t\t" << (size_t)dFPS << std::endl;
		baxh.col_offset = ((usebottom && !usetop) ? 400 : 0);
		double setWidth = ((usetop && !usebottom) ? 400 : 320);
		baxh.frame_width = setWidth;
		//bax << uint32_t(setWidth);
		strncpy(baxh.author,author,32);
		strncpy(baxh.info, info, 192);

		double setHeight = ((usetop != usebottom)?240:480);
		double curWidth = cap.get(cv::CAP_PROP_FRAME_WIDTH);
		double curHeight = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
		double scale = 1;
		if (curWidth > setWidth)
		{
			scale = setWidth / curWidth;
		}
		if (curHeight*scale > setHeight)
		{
			scale = setHeight / curHeight;
		}
		std::cout << "Video Resolution:\t" << (size_t)curWidth << "x" << (size_t)curHeight << std::endl;
		std::cout << "Max Resolution\t" << (size_t)setWidth << "x" << (size_t)setHeight << std::endl;
		if (scale != 1.0)
		{
			std::cout << "Resizing using scale:\t" << scale << std::endl;
			std::cout << "Resized Resolution:\t" << (size_t)curWidth*scale << "x" << (size_t)curHeight*scale << std::endl;
		}
		size_t hborder = (setWidth - (curWidth*scale)) / 2;
		size_t vborder = (setHeight - (curHeight*scale)) / 2;
		std::vector<Frame> vid;
		cv::Mat im;
		uint32_t *last = (uint32_t*)calloc(720*240, 2);

		while (cap.read(im) && vid.size() < maxFrames)
		{
			if (scale != 1.0)
				cv::resize(im, im, cv::Size(round(im.cols*scale), round(im.rows*scale))); // Resize if necessary

			Frame frame;
			frame.uncompressed= (uint16_t*)malloc(setWidth*setHeight*2);
			memcpy(frame.uncompressed, baseImage, setWidth*setHeight * 2);
			//uint16Fill(frame.uncompressed, bgColor, setWidth*setHeight);
			//memset(frame.uncompressed, bgColor, setWidth * setHeight);
			frame.uncompressed_size = setWidth * setHeight* 2;
			for (size_t x = 0; x < im.cols; x++)
			{
				for (size_t y = 0; y < im.rows; y++)
				{
					// Optimization should handle the extra variables but this makes it easier to troubleshoot
					size_t loc =(x+hborder) * (uint32_t)setHeight + (uint32_t)setHeight - (y+vborder) - 1;
					uchar* imColor = im.ptr(y, x);
					uint16_t rgb565 = BGR2RGB565(imColor);
					frame.uncompressed[loc] = rgb565;
				}
			}
			if (usetop && usebottom)
			{
				Portraitize(frame);
			}
			// Only track changes between frames
			uint32_t *current = reinterpret_cast<uint32_t*>(frame.uncompressed);
			for (size_t i = 0; i < frame.uncompressed_size / 4; i++)
			{
				uint32_t change = current[i] - last[i];
				last[i] = current[i];
				current[i] = change;
			}

			// LZ4 compression ftw
			size_t cb = LZ4_compressBound(frame.uncompressed_size);
			char* out_buffer = (char*)malloc(frame.uncompressed_size);
			frame.compressed_size = LZ4_compress_HC(reinterpret_cast<const char*> (frame.uncompressed),
				reinterpret_cast<char*> (out_buffer),
				frame.uncompressed_size, cb,0);
			frame.compressed = (char*)malloc(frame.compressed_size);
			memcpy(frame.compressed, out_buffer, frame.compressed_size);
			// remove uncompressed data, probably not needed on modern computers since we limit to 10 seconds
			free(out_buffer);
			free(frame.uncompressed);
			frame.uncompressed = NULL;
			frame.uncompressed_size = 0;
			vid.push_back(frame);
		}
		free(last);
		std::cout << vid.size() << " Frames processed out of " << maxFrames << " max" << std::endl;
		size_t FrameDataStart = 0x100 + (0x08 * vid.size());
//		std::cout << std::hex << "Frame Data starts at " << FrameDataStart << std::endl;
		size_t RunningLoc = FrameDataStart;
		baxh.frame_count = vid.size();
		if (usetop && usebottom)
			setWidth = 720;
		baxh.frame_width = setWidth;
		// process frames
		std::string ofile = video;
		ofile = ofile.substr(0,ofile.find_last_of('.'));
		ofile = ofile + ".bax";
		std::ofstream baxfile(ofile, std::ios::out | std::ios::binary);
		baxfile.write((const char*)&baxh, sizeof(baxh));
		for (auto frame : vid)
		{
			//std::cout << "Adding Frame MetaData";
			baxfile.write((char*)&RunningLoc, 4);
			baxfile.write((char*)&frame.compressed_size,4);
			RunningLoc += frame.compressed_size;
		}
		for (auto frame : vid)
		{
			baxfile.write(frame.compressed, frame.compressed_size);
			if (frame.compressed != NULL)
				free(frame.compressed);
			frame.compressed = NULL;
			if (frame.uncompressed != NULL)
				free(frame.uncompressed);
			frame.uncompressed = NULL;
		}
		baxfile.close();

		// write file
		std::cout << "Bax Generation Complete\n";
	}
	else
	{
		std::cout << "Failed to open video\n";
	}

}
