/*
 * xfrm4_mode_transport.c - Transport mode encapsulation for IPv4.
 *
 * Copyright (c) 2004-2006 Herbert Xu <herbert@gondor.apana.org.au>
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/stringify.h>
#include <net/dst.h>
#include <net/ip.h>
#include <net/xfrm.h>

#if IS_ENABLED(CONFIG_RALINK_HWCRYPTO)
#include "../xfrm/xfrm_mtk_symbols.h"
#endif

/* Add encapsulation header.
 *
 * The IP header will be moved forward to make space for the encapsulation
 * header.
 */
static int xfrm4_transport_output(struct xfrm_state *x, struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);
	int ihl = iph->ihl * 4;

#if IS_ENABLED(CONFIG_RALINK_HWCRYPTO)
	if (x->type->proto == IPPROTO_ESP && atomic_read(&esp_mtk_hardware)) {
		int header_len = 0;

		if (x->props.mode == XFRM_MODE_TUNNEL)
			header_len += sizeof(struct iphdr);

		if (x->encap) {
			struct xfrm_encap_tmpl *encap = x->encap;

			header_len += sizeof(struct udphdr);
			if (encap->encap_type == UDP_ENCAP_ESPINUDP_NON_IKE)
				header_len += 2 * sizeof(u32);
		}

		skb_set_network_header(skb, -header_len);
	} else
#endif
	skb_set_network_header(skb, -x->props.header_len);

	skb->mac_header = skb->network_header +
			  offsetof(struct iphdr, protocol);
	skb->transport_header = skb->network_header + ihl;
	__skb_pull(skb, ihl);
	memmove(skb_network_header(skb), iph, ihl);
	return 0;
}

/* Remove encapsulation header.
 *
 * The IP header will be moved over the top of the encapsulation header.
 *
 * On entry, skb->h shall point to where the IP header should be and skb->nh
 * shall be set to where the IP header currently is.  skb->data shall point
 * to the start of the payload.
 */
static int xfrm4_transport_input(struct xfrm_state *x, struct sk_buff *skb)
{
	int ihl = skb->data - skb_transport_header(skb);

	if (skb->transport_header != skb->network_header) {
		memmove(skb_transport_header(skb),
			skb_network_header(skb), ihl);
		skb->network_header = skb->transport_header;
	}
	ip_hdr(skb)->tot_len = htons(skb->len + ihl);
	skb_reset_transport_header(skb);
	return 0;
}

static struct xfrm_mode xfrm4_transport_mode = {
	.input = xfrm4_transport_input,
	.output = xfrm4_transport_output,
	.owner = THIS_MODULE,
	.encap = XFRM_MODE_TRANSPORT,
};

static int __init xfrm4_transport_init(void)
{
	return xfrm_register_mode(&xfrm4_transport_mode, AF_INET);
}

static void __exit xfrm4_transport_exit(void)
{
	int err;

	err = xfrm_unregister_mode(&xfrm4_transport_mode, AF_INET);
	BUG_ON(err);
}

module_init(xfrm4_transport_init);
module_exit(xfrm4_transport_exit);
MODULE_LICENSE("GPL");
MODULE_ALIAS_XFRM_MODE(AF_INET, XFRM_MODE_TRANSPORT);
