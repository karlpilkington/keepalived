/* 
 * Soft:        Keepalived is a failover program for the LVS project
 *              <www.linuxvirtualserver.org>. It monitor & manipulate
 *              a loadbalanced server pool using multi-layer checks.
 * 
 * Part:        Configuration file parser/reader. Place into the dynamic
 *              data structure representation the conf file representing
 *              the loadbalanced server pool.
 *  
 * Version:     $Id: check_parser.c,v 1.1.5 2004/01/25 23:14:31 acassen Exp $
 * 
 * Author:      Alexandre Cassen, <acassen@linux-vs.org>
 *              
 *              This program is distributed in the hope that it will be useful,
 *              but WITHOUT ANY WARRANTY; without even the implied warranty of
 *              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *              See the GNU General Public License for more details.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Copyright (C) 2001, 2002, 2003 Alexandre Cassen, <acassen@linux-vs.org>
 */

#include "check_parser.h"
#include "check_data.h"
#include "check_api.h"
#include "global_data.h"
#include "global_parser.h"
#include "parser.h"
#include "memory.h"
#include "utils.h"

/* global defs */
extern check_conf_data *check_data;
extern unsigned long mem_allocated;

/* SSL handlers */
static void
ssl_handler(vector strvec)
{
	check_data->ssl = alloc_ssl();
}
static void
sslpass_handler(vector strvec)
{
	check_data->ssl->password = set_value(strvec);
}
static void
sslca_handler(vector strvec)
{
	check_data->ssl->cafile = set_value(strvec);
}
static void
sslcert_handler(vector strvec)
{
	check_data->ssl->certfile = set_value(strvec);
}
static void
sslkey_handler(vector strvec)
{
	check_data->ssl->keyfile = set_value(strvec);
}

/* Virtual Servers handlers */
static void
vsg_handler(vector strvec)
{
	/* Fetch queued vsg */
	alloc_vsg(VECTOR_SLOT(strvec, 1));
	alloc_value_block(strvec, alloc_vsg_entry);
}
static void
vs_handler(vector strvec)
{
	alloc_vs(VECTOR_SLOT(strvec, 1), VECTOR_SLOT(strvec, 2));
}
static void
delay_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	vs->delay_loop = atoi(VECTOR_SLOT(strvec, 1)) * TIMER_HZ;
}
static void
lbalgo_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	char *str = VECTOR_SLOT(strvec, 1);
	int size = sizeof (vs->sched);

	memcpy(vs->sched, str, size);
}
static void
lbkind_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	char *str = VECTOR_SLOT(strvec, 1);

#ifdef _KRNL_2_2_
	if (!strcmp(str, "NAT"))
		vs->loadbalancing_kind = 0;
	else if (!strcmp(str, "DR"))
		vs->loadbalancing_kind = IP_MASQ_F_VS_DROUTE;
	else if (!strcmp(str, "TUN"))
		vs->loadbalancing_kind = IP_MASQ_F_VS_TUNNEL;
#else
	if (!strcmp(str, "NAT"))
		vs->loadbalancing_kind = IP_VS_CONN_F_MASQ;
	else if (!strcmp(str, "DR"))
		vs->loadbalancing_kind = IP_VS_CONN_F_DROUTE;
	else if (!strcmp(str, "TUN"))
		vs->loadbalancing_kind = IP_VS_CONN_F_TUNNEL;
#endif
	else
		syslog(LOG_INFO, "PARSER : unknown [%s] routing method.", str);
}
static void
natmask_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	inet_ston(VECTOR_SLOT(strvec, 1), &vs->nat_mask);
}
static void
pto_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	char *str = VECTOR_SLOT(strvec, 1);
	int size = sizeof (vs->timeout_persistence);

	memcpy(vs->timeout_persistence, str, size);
}
static void
pgr_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	inet_ston(VECTOR_SLOT(strvec, 1), &vs->granularity_persistence);
}
static void
proto_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	char *str = VECTOR_SLOT(strvec, 1);
	vs->service_type = (!strcmp(str, "TCP")) ? IPPROTO_TCP : IPPROTO_UDP;
}
static void
hasuspend_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	vs->ha_suspend = 1;
}
static void
virtualhost_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	vs->virtualhost = set_value(strvec);
}

/* Sorry Servers handlers */
static void
ssvr_handler(vector strvec)
{
	alloc_ssvr(VECTOR_SLOT(strvec, 1), VECTOR_SLOT(strvec, 2));
}

/* Real Servers handlers */
static void
rs_handler(vector strvec)
{
	alloc_rs(VECTOR_SLOT(strvec, 1), VECTOR_SLOT(strvec, 2));
}
static void
weight_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	real_server *rs = LIST_TAIL_DATA(vs->rs);
	rs->weight = atoi(VECTOR_SLOT(strvec, 1));
}
static void
inhibit_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	real_server *rs = LIST_TAIL_DATA(vs->rs);
	rs->inhibit = 1;
}
static void
notify_up_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	real_server *rs = LIST_TAIL_DATA(vs->rs);
	rs->notify_up = set_value(strvec);
}
static void
notify_down_handler(vector strvec)
{
	virtual_server *vs = LIST_TAIL_DATA(check_data->vs);
	real_server *rs = LIST_TAIL_DATA(vs->rs);
	rs->notify_down = set_value(strvec);
}

vector
check_init_keywords(void)
{
	keywords = vector_alloc();

	/* global definitions mapping */
	global_init_keywords();

	/* SSL mapping */
	install_keyword_root("SSL", &ssl_handler);
	install_keyword("password", &sslpass_handler);
	install_keyword("ca", &sslca_handler);
	install_keyword("certificate", &sslcert_handler);
	install_keyword("key", &sslkey_handler);

	/* Virtual server mapping */
	install_keyword_root("virtual_server_group", &vsg_handler);
	install_keyword_root("virtual_server", &vs_handler);
	install_keyword("delay_loop", &delay_handler);
	install_keyword("lb_algo", &lbalgo_handler);
	install_keyword("lvs_sched", &lbalgo_handler);
	install_keyword("lb_kind", &lbkind_handler);
	install_keyword("lvs_method", &lbkind_handler);
	install_keyword("nat_mask", &natmask_handler);
	install_keyword("persistence_timeout", &pto_handler);
	install_keyword("persistence_granularity", &pgr_handler);
	install_keyword("protocol", &proto_handler);
	install_keyword("ha_suspend", &hasuspend_handler);
	install_keyword("virtualhost", &virtualhost_handler);

	/* Real server mapping */
	install_keyword("sorry_server", &ssvr_handler);
	install_keyword("real_server", &rs_handler);
	install_sublevel();
	install_keyword("weight", &weight_handler);
	install_keyword("inhibit_on_failure", &inhibit_handler);
	install_keyword("notify_up", &notify_up_handler);
	install_keyword("notify_down", &notify_down_handler);

	/* Checkers mapping */
	install_checkers_keyword();
	install_sublevel_end();

	return keywords;
}
