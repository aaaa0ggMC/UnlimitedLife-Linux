#include <AGE/World/Components.h>
#include <AGE/World/Camera.h>
#include <AGE/World/Object.h>
#include <AGE/Input.h>
#include <alib-g3/aclock.h>
#include <iostream>
#include <vector>
#include <numbers>

#define sepl std::cout << "-----------------------------------" << std::endl
#define tm std::cout << "TimeCostMS:" << clk.getOffset() << std::endl;clk.clearOffset()
#define info(X,T) std::cout << X " * "#T << std::endl;tloop = (T)
#define loop for(uint64_t i = 0;i < tloop;++i)

using namespace age;
using namespace age::world;
using namespace age::world::comps;
using namespace alib::g3;

int main(){
  EntityManager em;
  EntityWrapper e (em.createEntity(),em);
  Camera cam (em);
  uint64_t tloop = 0;
  Clock clk;
  e.add<comps::Transform>();

  ///Create
  sepl;
  info("get()",1e8);
  loop{
    cam.transform();
    //e.get<comps::Transform>();
  }
  tm;
  return 0;
}

