#!/bin/bash
#
# Copyright 2010-2012,2014-2015 Ettus Research LLC
# Copyright 2021 Ettus Research, a National Instruments Company
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

set_static(){
link_name=("enp225s0f0" "enp225s0f1" "enp225s0f2" "enp225s0f3" "enp193s0f0" "enp193s0f1" "enp193s0f2" "enp193s0f3" "enp1s0f0" "enp1s0f1" "enp1s0f2" "enp1s0f3" "enp37s0f0" "enp37s0f1" "enp37s0f2" "enp37s0f3")
for link in "${link_name[@]}"
do
    sudo ethtool -G $link tx 4096 rx 4096
done
}
main(){
ETH_CARDS=($(ip link | grep -E -o '[0-9]+: [0-9­A-Z­a-z]+:' | sed 's/^[0-9]*: //;s/://'))

print_arry "${ETH_CARDS[@]}"
set_static

}

#echo ${ETH_CARDS[4]}
#ethtool -g ${VAR[4]} print 1
#print 1
#rate 4096
#set 1,2,3,4-7
#set on boot

print_arry(){
    i=0
    for elm in $@ 
    do
        echo $i: $elm
        ((i+=1))
    done
    return 0
}

#used to have main code at the top 
# '$@' is used to make the command line arguments of the script visible in the function

main "$@"; exit