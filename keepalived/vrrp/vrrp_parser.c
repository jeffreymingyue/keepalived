/* 
 * Soft:        Keepalived is a failover program for the LVS project
 *              <www.linuxvirtualserver.org>. It monitor & manipulate
 *              a loadbalanced server pool using multi-layer checks.
 * 
 * Part:        Configuration file parser/reader. Place into the dynamic
 *              data structure representation the conf file representing
 *              the loadbalanced server pool.
 *  
 * Version:     $Id: vrrp_parser.c,v 1.1.6 2004/02/21 02:31:28 acassen Exp $
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
 * Copyright (C) 2001-2004 Alexandre Cassen, <acassen@linux-vs.org>
 */

#include "vrrp_parser.h"
#include "vrrp_data.h"
#include "vrrp_sync.h"
#include "vrrp_index.h"
#include "vrrp_if.h"
#include "vrrp.h"
#include "global_data.h"
#include "global_parser.h"
#include "parser.h"
#include "memory.h"

/* global defs */
extern vrrp_conf_data *vrrp_data;
extern unsigned long mem_allocated;

/* Static addresses handler */
static void
static_addresses_handler(vector strvec)
{
	alloc_value_block(strvec, alloc_saddress);
}

/* Static routes handler */
static void
static_routes_handler(vector strvec)
{
	alloc_value_block(strvec, alloc_sroute);
}

/* VRRP handlers */
static void
vrrp_sync_group_handler(vector strvec)
{
	alloc_vrrp_sync_group(VECTOR_SLOT(strvec, 1));
}
static void
vrrp_group_handler(vector strvec)
{
	vrrp_sgroup *vgroup = LIST_TAIL_DATA(vrrp_data->vrrp_sync_group);
	vgroup->iname = read_value_block();
}
static void
vrrp_gnotify_backup_handler(vector strvec)
{
	vrrp_sgroup *vgroup = LIST_TAIL_DATA(vrrp_data->vrrp_sync_group);
	vgroup->script_backup = set_value(strvec);
	vgroup->notify_exec = 1;
}
static void
vrrp_gnotify_master_handler(vector strvec)
{
	vrrp_sgroup *vgroup = LIST_TAIL_DATA(vrrp_data->vrrp_sync_group);
	vgroup->script_master = set_value(strvec);
	vgroup->notify_exec = 1;
}
static void
vrrp_gnotify_fault_handler(vector strvec)
{
	vrrp_sgroup *vgroup = LIST_TAIL_DATA(vrrp_data->vrrp_sync_group);
	vgroup->script_fault = set_value(strvec);
	vgroup->notify_exec = 1;
}
static void
vrrp_gnotify_handler(vector strvec)
{
	vrrp_sgroup *vgroup = LIST_TAIL_DATA(vrrp_data->vrrp_sync_group);
	vgroup->script = set_value(strvec);
	vgroup->notify_exec = 1;
}
static void
vrrp_gsmtp_handler(vector strvec)
{
	vrrp_sgroup *vgroup = LIST_TAIL_DATA(vrrp_data->vrrp_sync_group);
	vgroup->smtp_alert = 1;
}
static void
vrrp_handler(vector strvec)
{
	alloc_vrrp(VECTOR_SLOT(strvec, 1));
}
static void
vrrp_state_handler(vector strvec)
{
	char *str = VECTOR_SLOT(strvec, 1);
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp_sgroup *vgroup = vrrp->sync;

	if (!strcmp(str, "MASTER")) {
		vrrp->wantstate = VRRP_STATE_MAST;
		vrrp->init_state = VRRP_STATE_MAST;
	}

	/* set eventual sync group */
	if (vgroup)
		vgroup->state = vrrp->wantstate;
}
static void
vrrp_int_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	char *name = VECTOR_SLOT(strvec, 1);
	vrrp->ifp = if_get_by_ifname(name);
}
static void
vrrp_track_int_handler(vector strvec)
{
	alloc_value_block(strvec, alloc_vrrp_track);
}
static void
vrrp_mcastip_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	inet_ston(VECTOR_SLOT(strvec, 1), &vrrp->mcast_saddr);
}
static void
vrrp_vrid_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp->vrid = atoi(VECTOR_SLOT(strvec, 1));

	if (VRRP_IS_BAD_VID(vrrp->vrid)) {
		syslog(LOG_INFO, "VRRP Error : VRID not valid !\n");
		syslog(LOG_INFO,
		       "             must be between 1 & 255. reconfigure !\n");
	} else
		alloc_vrrp_bucket(vrrp);
}
static void
vrrp_prio_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp->priority = atoi(VECTOR_SLOT(strvec, 1));

	if (VRRP_IS_BAD_PRIORITY(vrrp->priority)) {
		syslog(LOG_INFO, "VRRP Error : Priority not valid !\n");
		syslog(LOG_INFO,
		       "             must be between 1 & 255. reconfigure !\n");
		syslog(LOG_INFO, "             Using default value : 100\n");
		vrrp->priority = 100;
	}
}
static void
vrrp_adv_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp->adver_int = atoi(VECTOR_SLOT(strvec, 1));

	if (VRRP_IS_BAD_ADVERT_INT(vrrp->adver_int)) {
		syslog(LOG_INFO, "VRRP Error : Advert intervall not valid !\n");
		syslog(LOG_INFO,
		       "             must be between less than 1sec.\n");
		syslog(LOG_INFO, "             Using default value : 1sec\n");
		vrrp->adver_int = 1;
	}
	vrrp->adver_int *= TIMER_HZ;
}
static void
vrrp_debug_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp->debug = atoi(VECTOR_SLOT(strvec, 1));

	if (VRRP_IS_BAD_DEBUG_INT(vrrp->debug)) {
		syslog(LOG_INFO, "VRRP Error : Debug intervall not valid !\n");
		syslog(LOG_INFO, "             must be between 0-4\n");
		vrrp->debug = 0;
	}
}
static void
vrrp_preempt_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp->preempt = !vrrp->preempt;
}
static void
vrrp_notify_backup_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp->script_backup = set_value(strvec);
	vrrp->notify_exec = 1;
}
static void
vrrp_notify_master_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp->script_master = set_value(strvec);
	vrrp->notify_exec = 1;
}
static void
vrrp_notify_fault_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp->script_fault = set_value(strvec);
	vrrp->notify_exec = 1;
}
static void
vrrp_notify_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp->script = set_value(strvec);
	vrrp->notify_exec = 1;
}
static void
vrrp_smtp_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp->smtp_alert = 1;
}
static void
vrrp_lvs_syncd_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp->lvs_syncd_if = set_value(strvec);
}
static void
vrrp_garp_delay_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	vrrp->garp_delay = atoi(VECTOR_SLOT(strvec, 1)) * TIMER_HZ;
}
static void
vrrp_auth_type_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	char *str = VECTOR_SLOT(strvec, 1);

	if (!strcmp(str, "AH"))
		vrrp->auth_type = VRRP_AUTH_AH;
	else
		vrrp->auth_type = VRRP_AUTH_PASS;
}
static void
vrrp_auth_pass_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	char *str = VECTOR_SLOT(strvec, 1);
	int max_size = sizeof (vrrp->auth_data);
	int size;

	size = (strlen(str) >= max_size) ? max_size : strlen(str);
	memcpy(vrrp->auth_data, str, size);
}
static void
vrrp_vip_handler(vector strvec)
{
	vrrp_rt *vrrp = LIST_TAIL_DATA(vrrp_data->vrrp);
	char *buf;
	char *str = NULL;
	vector vec = NULL;
	int nbvip = 0;

	buf = (char *) MALLOC(MAXBUF);
	while (read_line(buf, MAXBUF)) {
		vec = alloc_strvec(buf);
		if (vec) {
			str = VECTOR_SLOT(vec, 0);
			if (!strcmp(str, EOB)) {
				free_strvec(vec);
				break;
			}

			if (VECTOR_SIZE(vec)) {
				nbvip++;
				if (nbvip > VRRP_MAX_VIP) {
					syslog(LOG_INFO,
					       "VRRP_Instance(%s) "
					       "trunc to the first %d VIPs.",
					       vrrp->iname, VRRP_MAX_VIP);
					syslog(LOG_INFO,
					       "  => Declare others VIPs into"
					       " the excluded vip block");
				} else
					alloc_vrrp_vip(vec);
			}

			free_strvec(vec);
		}
		memset(buf, 0, MAXBUF);
	}
	FREE(buf);
}
static void
vrrp_evip_handler(vector strvec)
{
	alloc_value_block(strvec, alloc_vrrp_evip);
}
static void
vrrp_vroutes_handler(vector strvec)
{
	alloc_value_block(strvec, alloc_vrrp_vroute);
}

vector
vrrp_init_keywords(void)
{
	keywords = vector_alloc();

	/* global definitions mapping */
	global_init_keywords();

	/* Static routes mapping */
	install_keyword_root("static_ipaddress", &static_addresses_handler);
	install_keyword_root("static_routes", &static_routes_handler);

	/* VRRP Instance mapping */
	install_keyword_root("vrrp_sync_group", &vrrp_sync_group_handler);
	install_keyword("group", &vrrp_group_handler);
	install_keyword("notify_backup", &vrrp_gnotify_backup_handler);
	install_keyword("notify_master", &vrrp_gnotify_master_handler);
	install_keyword("notify_fault", &vrrp_gnotify_fault_handler);
	install_keyword("notify", &vrrp_gnotify_handler);
	install_keyword("smtp_alert", &vrrp_gsmtp_handler);
	install_keyword_root("vrrp_instance", &vrrp_handler);
	install_keyword("state", &vrrp_state_handler);
	install_keyword("interface", &vrrp_int_handler);
	install_keyword("track_interface", &vrrp_track_int_handler);
	install_keyword("mcast_src_ip", &vrrp_mcastip_handler);
	install_keyword("virtual_router_id", &vrrp_vrid_handler);
	install_keyword("priority", &vrrp_prio_handler);
	install_keyword("advert_int", &vrrp_adv_handler);
	install_keyword("virtual_ipaddress", &vrrp_vip_handler);
	install_keyword("virtual_ipaddress_excluded", &vrrp_evip_handler);
	install_keyword("virtual_routes", &vrrp_vroutes_handler);
	install_keyword("preempt", &vrrp_preempt_handler);
	install_keyword("debug", &vrrp_debug_handler);
	install_keyword("notify_backup", &vrrp_notify_backup_handler);
	install_keyword("notify_master", &vrrp_notify_master_handler);
	install_keyword("notify_fault", &vrrp_notify_fault_handler);
	install_keyword("notify", &vrrp_notify_handler);
	install_keyword("smtp_alert", &vrrp_smtp_handler);
	install_keyword("lvs_sync_daemon_interface", &vrrp_lvs_syncd_handler);
	install_keyword("garp_master_delay", &vrrp_garp_delay_handler);
	install_keyword("authentication", NULL);
	install_sublevel();
	install_keyword("auth_type", &vrrp_auth_type_handler);
	install_keyword("auth_pass", &vrrp_auth_pass_handler);
	install_sublevel_end();

	return keywords;
}
