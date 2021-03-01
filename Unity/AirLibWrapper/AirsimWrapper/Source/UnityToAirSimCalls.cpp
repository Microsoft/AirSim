#include "UnityToAirSimCalls.h"

void StartServerThread(std::string sim_mode_name, int port_number)
{
	key = new SimHUD(sim_mode_name, port_number);
	key->BeginPlay();
}