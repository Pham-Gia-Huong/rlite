/*
 * N-1 ports management for normal uipcps.
 *
 * Copyright (C) 2015-2016 Nextworks
 * Author: Vincenzo Maffione <v.maffione@gmail.com>
 *
 * This file is part of rlite.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <climits>

#include "uipcp-normal.hpp"

using namespace std;


LowerFlow *
uipcp_rib::lfdb_find(rl_addr_t local_addr, rl_addr_t remote_addr)
{
    const LowerFlow *lf = _lfdb_find(local_addr, remote_addr);
    return const_cast<LowerFlow *>(lf);
}

const LowerFlow *
uipcp_rib::_lfdb_find(rl_addr_t local_addr, rl_addr_t remote_addr) const
{
    map<rl_addr_t, map<rl_addr_t, LowerFlow> >::const_iterator it
                                            = lfdb.find(local_addr);
    map<rl_addr_t, LowerFlow>::const_iterator jt;

    if (it == lfdb.end()) {
        return NULL;
    }

    jt = it->second.find(remote_addr);

    return jt == it->second.end() ? NULL : &jt->second;
}

/* The add method has overwrite semantic.
 * Returns true if something changed. */
bool
uipcp_rib::lfdb_add(const LowerFlow &lf)
{
    map<rl_addr_t, map<rl_addr_t, LowerFlow> >::iterator it
                                            = lfdb.find(lf.local_addr);
    string repr = static_cast<string>(lf);
    LowerFlow lfz = lf;

    lfz.age = 0;

    if (it == lfdb.end() || it->second.count(lf.remote_addr) == 0) {
        /* Not there, we need to add the entry. */
        lfdb[lf.local_addr][lf.remote_addr] = lfz;
        UPD(uipcp, "Lower flow %s added\n", repr.c_str());
        return true;
    }

    if (lfz == it->second[lfz.remote_addr] ||
            lfz.seqnum <= it->second[lfz.remote_addr].seqnum) {
        /* Entry is already there, and @lf is older. */
        return false;
    }

    it->second[lfz.remote_addr] = lfz; /* Update the entry */

    UPD(uipcp, "Lower flow %s updated\n", repr.c_str());

    return true;
}

/* Returns true if something changed. */
bool
uipcp_rib::lfdb_del(rl_addr_t local_addr, rl_addr_t remote_addr)
{
    map<rl_addr_t, map<rl_addr_t, LowerFlow> >::iterator it
                                            = lfdb.find(local_addr);
    map<rl_addr_t, LowerFlow>::iterator jt;

    if (it == lfdb.end()) {
        return false;
    }

    jt = it->second.find(remote_addr);

    if (jt == it->second.end()) {
        return false;
    }

    it->second.erase(jt);

    UPD(uipcp, "Lower flow %u-%u removed\n", (unsigned int)local_addr,
		(unsigned int)remote_addr);

    return true;
}

void
uipcp_rib::lfdb_update_local(const string& neigh_name)
{
    rl_addr_t remote_addr = lookup_neighbor_address(neigh_name);
    Neighbor *neigh = get_neighbor(neigh_name, false);
    LowerFlowList lfl;
    LowerFlow lf;
    CDAPMessage *sm = new CDAPMessage();

    if (remote_addr == 0 || neigh == NULL || !neigh->has_mgmt_flow()) {
        /* We still miss the address or the N-1 flow is not there. */
        return;
    }

    UPD(uipcp, "I will add lower flow %u-->%u for neigh %s\n",
               (unsigned)myaddr, (unsigned)remote_addr, neigh_name.c_str());

    lf.local_addr = myaddr;
    lf.remote_addr = remote_addr;
    lf.cost = 1;
    lf.seqnum = 1;
    lf.state = true;
    lf.age = 0;
    lfl.flows.push_back(lf);

    sm->m_create(gpb::F_NO_FLAGS, obj_class::lfdb, obj_name::lfdb, 0, 0, "");
    send_to_dst_addr(sm, myaddr, &lfl);
}

int
uipcp_rib::lfdb_handler(const CDAPMessage *rm, NeighFlow *nf)
{
    const char *objbuf;
    size_t objlen;
    bool add = true;

    if (rm->op_code != gpb::M_CREATE && rm->op_code != gpb::M_DELETE) {
        UPE(uipcp, "M_CREATE or M_DELETE expected\n");
        return 0;
    }

    if (rm->op_code == gpb::M_DELETE) {
        add = false;
    }

    rm->get_obj_value(objbuf, objlen);
    if (!objbuf) {
        UPE(uipcp, "M_START does not contain a nested message\n");
        abort();
        return 0;
    }

    LowerFlowList lfl(objbuf, objlen);
    LowerFlowList prop_lfl;
    bool modified = false;

    for (list<LowerFlow>::iterator f = lfl.flows.begin();
                                f != lfl.flows.end(); f++) {
        if (add) {
            if (lfdb_add(*f)) {
                modified = true;
                prop_lfl.flows.push_back(*f);
            }

        } else {
            if (lfdb_del(f->local_addr, f->remote_addr)) {
                modified = true;
                prop_lfl.flows.push_back(*f);
            }
        }
    }

    if (modified) {
        /* Send the received lower flows to the other neighbors. */
        remote_sync_obj_excluding(nf ? nf->neigh : NULL, add, obj_class::lfdb,
                                  obj_name::lfdb, &prop_lfl);

        /* Update the routing table. */
        spe.run(myaddr, this);
        pduft_sync();
    }

    return 0;
}

int
SPEngine::run(rl_addr_t local_addr, struct uipcp_rib *rib)
{
    /* Clean up state left from the previous run. */
    next_hops.clear();
    graph.clear();
    info.clear();

    /* Build the graph from the Lower Flow Database. */
    for (map<rl_addr_t, map<rl_addr_t, LowerFlow > >::const_iterator it
                = rib->lfdb.begin(); it != rib->lfdb.end(); it++) {
        for (map<rl_addr_t, LowerFlow>::const_iterator jt
                    = it->second.begin(); jt != it->second.end(); jt++) {
            const LowerFlow *revlf;

            revlf = rib->lfdb_find(jt->second.local_addr,
                                   jt->second.remote_addr);

            if (revlf == NULL || revlf->cost != jt->second.cost) {
                /* Something is wrong, this could be malicious or erroneous. */
                continue;
            }

            graph[jt->second.local_addr].push_back(Edge(jt->second.remote_addr,
                        jt->second.cost));
        }
    }

#if 1
    PV_S("Graph [%lu]:\n", rib->lfdb.size());
    for (map<rl_addr_t, list<Edge> >::iterator g = graph.begin();
                                            g != graph.end(); g++) {
        PV_S("%lu: {", (long unsigned)g->first);
        for (list<Edge>::iterator l = g->second.begin();
                                    l != g->second.end(); l++) {
            PV_S("(%lu, %u), ", (long unsigned)l->to, l->cost);
        }
        PV_S("}\n");
    }
#endif

    /* Initialize the per-node info map. */
    for (map<rl_addr_t, list<Edge> >::iterator g = graph.begin();
                                            g != graph.end(); g++) {
        struct Info inf;

        inf.dist = UINT_MAX;
        inf.visited = false;

        info[g->first] = inf;
    }
    info[local_addr].dist = 0;

    for (;;) {
        rl_addr_t min = UINT_MAX;
        unsigned int min_dist = UINT_MAX;

        /* Select the closest node from the ones in the frontier. */
        for (map<rl_addr_t, Info>::iterator i = info.begin();
                                        i != info.end(); i++) {
            if (!i->second.visited && i->second.dist < min_dist) {
                min = i->first;
                min_dist = i->second.dist;
            }
        }

        if (min_dist == UINT_MAX) {
            break;
        }

        assert(min != UINT_MAX);

        PV_S("Selecting node %lu\n", (long unsigned)min);

        list<Edge>& edges = graph[min];
        Info& info_min = info[min];

        info_min.visited = true;

        for (list<Edge>::iterator edge = edges.begin();
                                edge != edges.end(); edge++) {
            Info& info_to = info[edge->to];

            if (info_to.dist > info_min.dist + edge->cost) {
                info_to.dist = info_min.dist + edge->cost;
                next_hops[edge->to] = (min == local_addr) ? edge->to :
                                                    next_hops[min];
            }
        }
    }

    PV_S("Dijkstra result:\n");
    for (map<rl_addr_t, Info>::iterator i = info.begin();
                                    i != info.end(); i++) {
        PV_S("    Address: %lu, Dist: %u, Visited %u\n",
                (long unsigned)i->first, i->second.dist,
                (i->second.visited));
    }

    PV_S("Routing table:\n");
    for (map<rl_addr_t, rl_addr_t>::iterator h = next_hops.begin();
                                        h != next_hops.end(); h++) {
        PV_S("    Address: %lu, Next hop: %lu\n",
             (long unsigned)h->first, (long unsigned)h->second);
    }

    return 0;
}

int
uipcp_rib::pduft_sync()
{
    map<rl_addr_t, rl_port_t> next_hop_to_port_id;

    /* Flush previous entries. */
    uipcp_pduft_flush(uipcp, uipcp->id);

    /* Precompute the port-ids corresponding to all the possible
     * next-hops. */
    for (map<rl_addr_t, rl_addr_t>::iterator r = spe.next_hops.begin();
                                        r !=  spe.next_hops.end(); r++) {
        map<string, Neighbor*>::iterator neigh;
        string neigh_name;

        if (next_hop_to_port_id.count(r->second)) {
            continue;
        }

        neigh_name = static_cast<string>(
                                lookup_neighbor_by_address(r->second));
        if (neigh_name == string()) {
            UPE(uipcp, "Could not find neighbor with address %lu\n",
                    (long unsigned)r->second);
            continue;
        }

        neigh = neighbors.find(neigh_name);

        if (neigh == neighbors.end()) {
            UPE(uipcp, "Could not find neighbor with name %s\n",
                    neigh_name.c_str());
            continue;
        }

        /* Just take one for now. */
        assert(neigh->second->has_mgmt_flow());
        next_hop_to_port_id[r->second] = neigh->second->mgmt_conn()->port_id;
    }

    /* Generate PDUFT entries. */
    for (map<rl_addr_t, rl_addr_t>::iterator r = spe.next_hops.begin();
                                        r !=  spe.next_hops.end(); r++) {
            rl_port_t port_id = next_hop_to_port_id[r->second];
            int ret = uipcp_pduft_set(uipcp, uipcp->id, r->first,
                                      port_id);
            if (ret) {
                UPE(uipcp, "Failed to insert %lu --> %u PDUFT entry\n",
                    (long unsigned)r->first, port_id);
            } else {
                UPV(uipcp, "Add PDUFT entry %lu --> %u\n",
                    (long unsigned)r->first, port_id);
            }
    }

    return 0;
}

void
age_incr_cb(struct uipcp *uipcp, void *arg)
{
    struct uipcp_rib *rib = (struct uipcp_rib *)arg;
    ScopeLock(rib->lock);
    bool discarded = false;

    for (map<rl_addr_t, map< rl_addr_t, LowerFlow > >::iterator it
                = rib->lfdb.begin(); it != rib->lfdb.end(); it++) {
        list<map<rl_addr_t, LowerFlow >::iterator> discard_list;

        if (it->first == rib->myaddr) {
            /* Don't age the entries generated by me, we pretend they
             * are always refreshed. */
            continue;
        }

        for (map<rl_addr_t, LowerFlow >::iterator jt = it->second.begin();
                                                jt != it->second.end(); jt++) {
            jt->second.age += RL_AGE_INCR_INTERVAL;

            if (jt->second.age > RL_AGE_MAX) {
                /* Insert this into the list of entries to be discarded. */
                discard_list.push_back(jt);
                discarded = true;
            }
        }

        for (list<map<rl_addr_t, LowerFlow >::iterator>::iterator dit
                    = discard_list.begin(); dit != discard_list.end(); dit++) {
            UPI(rib->uipcp, "Discarded lower-flow %s\n",
                            static_cast<string>((*dit)->second).c_str());
            it->second.erase(*dit);
        }
    }

    if (discarded) {
        /* Update the routing table. */
        rib->spe.run(rib->myaddr, rib);
        rib->pduft_sync();
    }

    /* Reschedule */
    rib->age_incr_tmrid = uipcp_loop_schedule(uipcp,
                                        RL_AGE_INCR_INTERVAL * 1000,
                                        age_incr_cb, rib);
}
