#include <stdint.h>
#include "dhcp.h"
#include "ConfigData.h"

void w7500x_dhcp_assign(void)
{
	DevConfig *value = get_DevConfig_pointer();
	wiz_NetInfo gWIZNETINFO;

	getIPfromDHCP(gWIZNETINFO.ip);
	getGWfromDHCP(gWIZNETINFO.gw);
	getSNfromDHCP(gWIZNETINFO.sn);
	getDNSfromDHCP(gWIZNETINFO.dns);

	get_DevConfig_value(gWIZNETINFO.mac, value->network_info_common.mac, sizeof(gWIZNETINFO.mac[0]) * 6);
	set_DevConfig_value(value->network_info_common.local_ip, gWIZNETINFO.ip, sizeof(value->network_info_common.local_ip));
	set_DevConfig_value(value->network_info_common.gateway, gWIZNETINFO.gw, sizeof(value->network_info_common.gateway));
	set_DevConfig_value(value->network_info_common.subnet, gWIZNETINFO.sn, sizeof(value->network_info_common.subnet));
	set_DevConfig_value(value->options.dns_server_ip, gWIZNETINFO.dns, sizeof(value->options.dns_server_ip));
	if(value->options.dhcp_use)
		gWIZNETINFO.dhcp = NETINFO_DHCP;
	else
		gWIZNETINFO.dhcp = NETINFO_STATIC;

	ctlnetwork(CN_SET_NETINFO, (void*) &gWIZNETINFO);

//	display_Net_Info();
//	printf("DHCP LEASED TIME : %d sec. \r\n", getDHCPLeasetime());
}

void w7500x_dhcp_conflict(void)
{
	// TODO
	;
}

