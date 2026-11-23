#pragma once

#include "block.h"
#include <functional>


struct AutomaticRerunSystem{
	std::unique_ptr<Block> simulator;
	
	AutomaticRerunSystem();
	
	void addCheck(std::function<bool(void)>);
	void addCheck(bool);
	
	void go();
	void step();
	
	bool foundBug();
	
	void dummyFunction(); // set breakpoint here
	
private:
	int leftBound = 0;
	int rightBound = 2;
	bool anyCheckFired = false;
	size_t counter = 0;
	
	void resetSim();
};
