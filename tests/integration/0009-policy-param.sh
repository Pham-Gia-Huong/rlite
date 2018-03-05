#!/bin/sh

# Create a normal IPCP and test the dif-policy-param-mod command
rlite-ctl ipcp-create x normal dd || exit 1
rlite-ctl ipcp-enroller-enable x || exit 1

# Expect success on the following ones
rlite-ctl dif-policy-param-mod dd addralloc nack-wait-secs 4 || exit 1
rlite-ctl dif-policy-param-mod dd enrollment timeout 300 || exit 1
rlite-ctl dif-policy-param-mod dd enrollment keepalive 4 || exit 1
rlite-ctl dif-policy-param-mod dd enrollment keepalive-thresh 2 || exit 1
rlite-ctl dif-policy-param-mod dd flowalloc force-flow-control true || exit 1
rlite-ctl dif-policy-param-mod dd resalloc reliable-flows true || exit 1
rlite-ctl dif-policy-param-mod dd resalloc reliable-n-flows true || exit 1
rlite-ctl dif-policy-param-mod dd resalloc broadcast-enroller true || exit 1
rlite-ctl dif-policy-param-mod dd ribd refresh-intval 10 || exit 1
rlite-ctl dif-policy-param-mod dd routing age-incr-intval 10 || exit 1
rlite-ctl dif-policy-param-mod dd addralloc nack-wait-secs 1 || exit 1
rlite-ctl dif-policy-param-mod dd addralloc nack-wait-secs 99 || exit 1

# Expect failure on the following ones
rlite-ctl dif-policy-param-mod dd wrong-component timeout 300 && exit 1
rlite-ctl dif-policy-param-mod dd enrollment wrong-parameter 300 && exit 1
rlite-ctl dif-policy-param-mod dd enrollment timeout wrong-value && exit 1
rlite-ctl dif-policy-param-mod dd resalloc reliable-flows 1023 && exit 1
rlite-ctl dif-policy-param-mod dd ribd refresh-intval false && exit 1
rlite-ctl dif-policy-param-mod dd addralloc nack-wait-secs 0 && exit 1
rlite-ctl dif-policy-param-mod dd addralloc nack-wait-secs 100 && exit 1
exit 0
