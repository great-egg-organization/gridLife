#include "automaticRerunSystem.h"
#include <stdexcept>


AutomaticRerunSystem::AutomaticRerunSystem() {
	simulator = std::make_unique<Block>(this);
}

#ifdef NDEBUG
void AutomaticRerunSystem::addCheck(std::function<bool(void)>){}
void AutomaticRerunSystem::addCheck(bool){}
#else
void AutomaticRerunSystem::addCheck(std::function<bool(void)> fun){
	if (foundBug()){
		++counter;
		dummyFunction();
		return;
	}
	if (anyCheckFired && rightBound == counter){
		fun();
		throw std::runtime_error("I permit myself to use exceptions here");
	}
	if (rightBound < counter){
		assert(!anyCheckFired);
		rightBound = 1.5*rightBound + 1;
	}
	if ((leftBound + rightBound)/2 == counter){
		if (fun()){
		  leftBound = counter;
		}
		else{
		  rightBound = counter;
			throw std::runtime_error("I permit myself to use exceptions here");
		}
	}
	++counter;
}

void AutomaticRerunSystem::addCheck(bool flag){
	if (!flag) {
		--counter;
		rightBound = counter;
		throw std::runtime_error("I permit myself to use exceptions here");
	}
}
#endif

void AutomaticRerunSystem::go(){
	while (true) {step();}
}

#ifdef NDEBUG
void AutomaticRerunSystem::step(){
	simulator->step();
}
#else
void AutomaticRerunSystem::step(){
	try {
		simulator->step();
	}
	catch (std::runtime_error e){
		assert(!foundBug());
		resetSim();
	}
}
#endif

bool AutomaticRerunSystem::foundBug(){
	return anyCheckFired && rightBound - leftBound < 2 && counter == rightBound - 1;
}

void AutomaticRerunSystem::dummyFunction(){
	return;
}

void AutomaticRerunSystem::resetSim(){
	anyCheckFired = true;
  counter = 0;
  simulator.reset(new Block(this));
}
