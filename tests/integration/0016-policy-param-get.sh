#!/bin/sh

# Create a normal IPCP and test the dif-policy-param-list command
rlite-ctl ipcp-create x normal dd || exit 1
rlite-ctl ipcp-enroller-enable x || exit 1

# List per-component parameters, checking that the number of lines is correct
rlite-ctl dif-policy-param-list dd
rlite-ctl dif-policy-param-list dd | wc -l | grep -q "\<18\>" || exit 1
rlite-ctl dif-policy-param-list dd addralloc | wc -l | grep -q "\<1\>" || exit 1
rlite-ctl dif-policy-param-list dd dft | wc -l | grep -q "\<1\>" || exit 1
rlite-ctl dif-policy-param-list dd enrollment | wc -l | grep -q "\<4\>" || exit 1
rlite-ctl dif-policy-param-list dd flowalloc | wc -l | grep -q "\<6\>" || exit 1
rlite-ctl dif-policy-param-list dd resalloc | wc -l | grep -q "\<3\>" || exit 1
rlite-ctl dif-policy-param-list dd routing | wc -l | grep -q "\<2\>" || exit 1
rlite-ctl dif-policy-param-list dd ribd | wc -l | grep -q "\<1\>" || exit 1

# Run a list of set operations followed by a correspondent get, checking
# that the value got stored in the RIB.
rlite-ctl dif-policy-param-mod dd addralloc nack-wait-secs 76 || exit 1
rlite-ctl dif-policy-param-list dd addralloc nack-wait-secs | grep 76 || exit 1
rlite-ctl dif-policy-param-mod dd dft replicas r1,r2,r3,r4,r5 || exit 1
rlite-ctl dif-policy-param-list dd dft replicas | grep "r1,r2,r3,r4,r5" || exit 1
rlite-ctl dif-policy-param-mod dd enrollment timeout 300 || exit 1
rlite-ctl dif-policy-param-list dd enrollment timeout | grep 300 || exit 1
rlite-ctl dif-policy-param-mod dd enrollment keepalive 478 || exit 1
rlite-ctl dif-policy-param-list dd enrollment keepalive | grep 478 || exit 1
rlite-ctl dif-policy-param-mod dd enrollment keepalive-thresh 21 || exit 1
rlite-ctl dif-policy-param-list dd enrollment keepalive-thresh | grep 21 || exit 1
rlite-ctl dif-policy-param-mod dd enrollment auto-reconnect false || exit 1
rlite-ctl dif-policy-param-list dd enrollment auto-reconnect | grep false || exit 1
rlite-ctl dif-policy-param-mod dd flowalloc force-flow-control true || exit 1
rlite-ctl dif-policy-param-list dd flowalloc force-flow-control | grep true || exit 1
rlite-ctl dif-policy-param-mod dd flowalloc initial-a 41 || exit 1
rlite-ctl dif-policy-param-list dd flowalloc initial-a | grep 41 || exit 1
rlite-ctl dif-policy-param-mod dd flowalloc initial-credit 184 || exit 1
rlite-ctl dif-policy-param-list dd flowalloc initial-credit | grep 184 || exit 1
rlite-ctl dif-policy-param-mod dd flowalloc initial-rtx-timeout 1791 || exit 1
rlite-ctl dif-policy-param-list dd flowalloc initial-rtx-timeout | grep 1791 || exit 1
rlite-ctl dif-policy-param-mod dd flowalloc max-cwq-len 2961 || exit 1
rlite-ctl dif-policy-param-mod dd flowalloc max-rtxq-len 915 || exit 1
rlite-ctl dif-policy-param-list dd flowalloc max-cwq-len | grep 2961 || exit 1
rlite-ctl dif-policy-param-list dd flowalloc max-rtxq-len | grep 915 || exit 1
rlite-ctl dif-policy-param-mod dd resalloc reliable-flows true || exit 1
rlite-ctl dif-policy-param-list dd resalloc reliable-flows | grep true || exit 1
rlite-ctl dif-policy-param-mod dd resalloc reliable-n-flows true || exit 1
rlite-ctl dif-policy-param-list dd resalloc reliable-n-flows | grep true || exit 1
rlite-ctl dif-policy-param-mod dd resalloc broadcast-enroller true || exit 1
rlite-ctl dif-policy-param-list dd resalloc broadcast-enroller | grep true || exit 1
rlite-ctl dif-policy-param-mod dd ribd refresh-intval 916 || exit 1
rlite-ctl dif-policy-param-list dd ribd refresh-intval | grep 916 || exit 1
rlite-ctl dif-policy-param-mod dd routing age-incr-intval 107 || exit 1
rlite-ctl dif-policy-param-mod dd routing age-max 771 || exit 1
rlite-ctl dif-policy-param-list dd routing age-incr-intval | grep 107 || exit 1
rlite-ctl dif-policy-param-list dd routing age-max | grep 771 || exit 1

# Expect failure on the following ones
rlite-ctl dif-policy-param-list dd wrong-component timeout 300 && exit 1
rlite-ctl dif-policy-param-list dd enrollment wrong-parameter 300 && exit 1
exit 0
