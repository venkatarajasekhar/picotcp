START_TEST (arp_check_pending_test)
{
    pico_stack_init();
    pending_timer_on = 1;
    check_pending(0, NULL);
    fail_unless(pending_timer_on == 0);
}
END_TEST

START_TEST (arp_update_max_arp_reqs_test)
{
    pico_stack_init();
    max_arp_reqs = 0;
    usleep((PICO_ARP_INTERVAL * 2) * 1000);
    pico_stack_tick();
    fail_unless(max_arp_reqs > 0);
}
END_TEST

START_TEST (arp_compare_test)
{
    struct pico_arp a,b;
    char ipstr[] = "192.168.1.1";

    memset(&a, 0, sizeof(a));
    pico_string_to_ipv4(ipstr, &b.ipv4.addr);

    fail_unless(arp_compare(&a, &b) == -1);
    fail_unless(arp_compare(&b, &a) == 1);
    fail_unless(arp_compare(&a, &a) == 0);
}
END_TEST

START_TEST (arp_lookup_test)
{
    struct pico_ip4 ip;
    struct pico_eth *eth = NULL;
    char ipstr[] = "192.168.1.1";
    struct pico_arp entry;

    eth = pico_arp_lookup(&ip);
    fail_unless(eth == NULL);

    pico_string_to_ipv4(ipstr, &ip.addr);
    entry.ipv4 = ip;

    pico_stack_init();
    pico_arp_add_entry(&entry);
    entry.arp_status = PICO_ARP_STATUS_STALE;
    eth = pico_arp_lookup(&ip);
    fail_unless(eth == NULL);
}
END_TEST

START_TEST (arp_expire_test)
{
    struct pico_arp entry;
    entry.arp_status = PICO_ARP_STATUS_REACHABLE;

    arp_expire(0, &entry);
    fail_unless(entry.arp_status == PICO_ARP_STATUS_STALE);
}
END_TEST

static struct pico_frame *init_frame(struct pico_device * dev)
{
    struct pico_frame *f = pico_frame_alloc(PICO_SIZE_ETHHDR + PICO_SIZE_ARPHDR);
    f->net_hdr = f->buffer + PICO_SIZE_ETHHDR;
    f->datalink_hdr = f->buffer;
    f->dev = dev;

    return f;
}

START_TEST (arp_receive_test)
{
    struct mock_device *mock;
    struct pico_frame *f = NULL;
    struct pico_arp_hdr *ah = NULL;
    struct pico_eth_hdr *eh = NULL;
    uint8_t macaddr1[6] = {
        0, 0, 0, 0xa, 0xb, 0xf
    };
    uint8_t macaddr2[6] = {
        0, 0, 0, 0xc, 0xd, 0xf
    };
    struct pico_ip4 netmask = {
        .addr = long_be(0xffffff00)
    };
    struct pico_ip4 ip1 = {
        .addr = long_be(0x0A28000A)
    };
    struct pico_ip4 ip2 = {
        .addr = long_be(0x0A28000B)
    };

    pico_stack_init();

    /* Create mock device */
    mock = pico_mock_create(macaddr1);
    fail_if(!mock, "MOCK DEVICE creation failed");
    fail_if(pico_ipv4_link_add(mock->dev, ip1, netmask), "add link to mock device failed");

    /* Normal ARP request */
    f = init_frame(mock->dev);
    eh = (struct pico_eth_hdr *) f->datalink_hdr;
    ah = (struct pico_arp_hdr *) f->net_hdr;

    memcpy(eh->saddr, macaddr2, PICO_SIZE_ETH);
    memcpy(eh->daddr, PICO_ETHADDR_ALL, PICO_SIZE_ETH);
    eh->proto = PICO_IDETH_ARP;

    ah->htype  = PICO_ARP_HTYPE_ETH;
    ah->ptype  = PICO_IDETH_IPV4;
    ah->hsize  = PICO_SIZE_ETH;
    ah->psize  = PICO_SIZE_IP4;
    ah->opcode = PICO_ARP_REQUEST;
    memcpy(ah->s_mac, macaddr2, PICO_SIZE_ETH);
    ah->src.addr = ip2.addr;
    ah->dst.addr = ip1.addr;
    fail_unless(pico_arp_receive(f) == 0);

    /* net_hdr is a nullpointer */
    f = init_frame(mock->dev);
    f->net_hdr = NULL;
    fail_unless(pico_arp_receive(f) == -1);

    /* wrong hardware type */
    f = init_frame(mock->dev);
    ah = (struct pico_arp_hdr *) f->net_hdr;
    ah->htype = 0;
    fail_unless(pico_arp_receive(f) == -1);

    /* wrong protocol type */
    f = init_frame(mock->dev);
    ah = (struct pico_arp_hdr *) f->net_hdr;
    ah->ptype = 0;
    fail_unless(pico_arp_receive(f) == -1);

    /* source mac address is multicast */
    f = init_frame(mock->dev);
    ah = (struct pico_arp_hdr *) f->net_hdr;
    ah->s_mac[0] = 0x01;
    fail_unless(pico_arp_receive(f) == -1);
}
END_TEST
