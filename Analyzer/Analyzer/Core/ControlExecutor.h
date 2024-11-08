﻿#ifndef ANALYZER_CONTROLEXECUTOR_H
#define ANALYZER_CONTROLEXECUTOR_H
#include <thread>
#include <queue>
#include <mutex>
namespace AVSAnalyzer {
	class Scheduler;
	class AvPullStream;
	class AvPushStream;
	class GenerateAlarm;
	class Analyzer;
	struct Control;

	struct VideoFrame
	{
	public:
		enum VideoFrameType
		{
			BGR = 0,
			YUV420P,

		};
		VideoFrame(VideoFrameType type, int size, int width, int height) {
			this->type = type;
			this->size = size;
			this->width = width;
			this->height = height;
			this->data = new uint8_t[this->size];

		}
		~VideoFrame() {
			delete[] this->data;
			this->data = nullptr;
		}

		VideoFrameType type;
		int size;
		int width;
		int height;
		uint8_t* data;
		bool happen = false;// 是否发生事件
		float happenScore = 0;// 发生事件的分数


	};

	class ControlExecutor
	{
	public:

        /*防止隐式转换, 确保需要显式构造。例如有函数定义为:
         * doSomeThing(ControlExecutor()); 参数为类构造函数 ControlExecutor(Scheduler* scheduler, Control* control);
         * 正确的显示调用：doSomeThing(ControlExecutor(scheduler, contorl));
         * 避免这样的：doSomeThing(scheduler, contorl);*/ 
		explicit ControlExecutor(Scheduler* scheduler, Control* control);

		~ControlExecutor();
	public:
		static void decodeAndAnalyzeVideoThread(void* arg);// 解码视频帧和实时分析视频帧
	public:
		bool start(std::string& msg);

		bool getState();
		void setState_remove();
	public:
		Control* mControl;
		Scheduler* mScheduler;
		AvPullStream* mPullStream;
		AvPushStream* mPushStream;
		GenerateAlarm* mGenerateAlarm;
		Analyzer* mAnalyzer;

	private:
		bool mState = false;
		std::vector<std::thread*> mThreads;

	};
}
#endif //ANALYZER_CONTROLEXECUTOR_H
