﻿#include "multi_think.h"

#include "tt.h"
#include "uci.h"
#include "types.h"
#include "search.h"

#include <thread>

void MultiThink::go_think()
{
	// Keep a copy to restore the Options settings later.
	auto oldOptions = Options;

	// When using the constant track, it takes a lot of time to perform on the fly & the part to access the file is
	// Since it is not thread safe, it is guaranteed here that it is being completely read in memory.
	Options["BookOnTheFly"] = std::string("false");

	// Read evaluation function, etc.
	// In the case of the learn command, the value of the evaluation function may be corrected after reading the evaluation function, so
	// Skip memory corruption check.
	Eval::init_NNUE();

	// Call the derived class's init().
	init();

        // init global vars
        Tablebases::init();

        // About Search::Limits
        // Be careful because this member variable is global and affects other threads.
        {
          auto& limits = Search::Limits;

          // Make the search equivalent to the "go infinite" command. (Because it is troublesome if time management is done)
          limits.infinite = true;

          // Since PV is an obstacle when displayed, erase it.
          limits.silent = true;

          // If you use this, it will be compared with the accumulated nodes of each thread. Therefore, do not use it.
          limits.nodes = 0;

          // depth is also processed by the one passed as an argument of Learner::search().
          limits.depth = 0;
        }

	// The loop upper limit is set with set_loop_max().
	loop_count = 0;
	done_count = 0;

	// Create threads as many as Options["Threads"] and start thinking.
	std::vector<std::thread> threads;
	auto thread_num = (size_t)Options["Threads"];

	// Secure end flag of worker thread
        threads_finished=0;

	// start worker thread
	for (size_t i = 0; i < thread_num; ++i)
	{
		threads.push_back(std::thread([i, this]
		{
			// exhaust all processor threads.
			WinProcGroup::bindThisThread(i);

			// execute the overridden process
			this->thread_worker(i);

			// Set the end flag because the thread has ended
			this->threads_finished++;
		}));
	}

	// wait for all threads to finish
	// for (auto& th :threads)
	// th.join();
	// If you write like, the thread will rush here while it is still working,
	// During that time, callback_func() cannot be called and you cannot save.
	// Therefore, you need to check the end flag yourself.

	// function to determine if all threads have finished
	auto threads_done = [&]()
	{
		return threads_finished == thread_num;
	};

	// Call back if the callback function is set.
	auto do_a_callback = [&]()
	{
		if (callback_func)
			callback_func();
	};


	for (uint64_t i = 0 ; ; )
	{
		// If all threads have finished, exit the loop.
		if (threads_done())
			break;

		sleep(1000);

		// callback_func() is called every callback_seconds.
		if (++i == callback_seconds)
		{
			do_a_callback();
			// Since I am returning from ↑, I reset the counter, so
			// no matter how long it takes to save() etc. in do_a_callback()
			// The next call will take a certain amount of time.
			i = 0;
		}
	}

	// Last save.
	std::cout << std::endl << "finalize..";

	// do_a_callback();
	// → It should be saved by the caller, so I feel that it is not necessary here.

	// It is possible that the exit code of the thread is running but the exit code of the thread is running, so
	// We need to wait for the end with join().
	for (auto& th : threads)
		th.join();

	// The file writing thread etc. are still running only when all threads are finished
	// Since the work itself may not have completed, output only that all threads have finished.
	std::cout << "all threads are joined." << std::endl;

	// Restored because Options were rewritten.
	// Restore the handler because the handler will not start unless you assign a value.
	for (auto& s : oldOptions)
		Options[s.first] = std::string(s.second);

}
