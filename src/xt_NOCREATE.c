/*
 * Copyright (c) 2010 Mathew Heard <mheard@x4b.net>
 *
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/skbuff.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter/x_tables.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include "xt_NOCREATE.h"

static unsigned int
nocreate_tg(struct sk_buff *skb, const struct xt_action_param *par)
{
	const struct xt_nocreate_target_info *info;
	enum ip_conntrack_info ctinfo;
	struct nf_conn * tmpl = nf_ct_get(skb, &ctinfo);
	if (tmpl == NULL) {
		info = par->targinfo;
		atomic_inc(&info->ct->ct_general.use);
		nf_ct_set(skb, info->ct, IP_CT_NEW);
	} else if(likely(nf_ct_is_template(tmpl))){
		tmpl->status |= IPS_NOCREATE;
	}

	return XT_CONTINUE;
}

static int nocreate_chk(const struct xt_tgchk_param *par)
{
	struct xt_nocreate_target_info *info = par->targinfo;
	struct nf_conn *ct;
	struct nf_conntrack_zone zone;
	int ret;	

	ret = nf_ct_netns_get(par->net, par->family);
	if (ret < 0){
		pr_info_ratelimited("cannot load conntrack support for proto=%u\n",
				    par->family);
        	goto err1;
	}
	
	memset(&zone, 0, sizeof(zone));
	zone.dir = NF_CT_DEFAULT_ZONE_DIR;

	ct = nf_ct_tmpl_alloc(par->net, &zone, GFP_KERNEL);
	if (!ct) {
		ret = -ENOMEM;
		goto err;
	}
	
	__set_bit(IPS_CONFIRMED_BIT, &ct->status);
	__set_bit(IPS_NOCREATE_BIT, &ct->status);
	
	nf_conntrack_get(&ct->ct_general);

	info->ct = ct;
err:
	return ret;
err1:
//	nf_ct_l3proto_module_put(par->family);
	return ret;
}

static void xt_nocreate_tg_destroy(const struct xt_tgdtor_param *par,
			     struct xt_nocreate_target_info *info)
{
	nf_ct_put(info->ct);
	nf_ct_netns_put(par->net, par->family);
}

static void xt_nocreate_tg_destroy_v0(const struct xt_tgdtor_param *par)
{
	struct xt_nocreate_target_info *info = par->targinfo;

	xt_nocreate_tg_destroy(par, info);
}

static struct xt_target nocreate_tg_reg __read_mostly = {
	.name		= "NOCREATE",
	.revision	= 0,
	.family		= NFPROTO_UNSPEC,
	.checkentry	= nocreate_chk,
	.target		= nocreate_tg,
	.destroy	= xt_nocreate_tg_destroy_v0,
	.targetsize     = sizeof(struct xt_nocreate_target_info),
	.table		= "raw",
	.me		= THIS_MODULE,
};

static int __init xt_ct_tg_init(void)
{
	int ret;

	ret = xt_register_target(&nocreate_tg_reg);
	if (ret < 0)
		return ret;

	return 0;
}

static void __exit xt_ct_tg_exit(void)
{
	xt_unregister_target(&nocreate_tg_reg);
}

module_init(xt_ct_tg_init);
module_exit(xt_ct_tg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Xtables: nocreate target");
MODULE_ALIAS("ipt_NOCREATE");
MODULE_ALIAS("ip6t_NOCREATE");
