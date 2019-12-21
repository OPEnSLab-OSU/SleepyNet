# SleepyNet

[![Build Status](https://travis-ci.org/OPEnSLab-OSU/SleepyNet.svg?branch=master)](https://travis-ci.org/OPEnSLab-OSU/SleepyNet)

SleepyNet is an IoT networking stack designed to be low-footprint, low-power, and intuitive. In other words, SleepyNet makes it easy to periodically send data from wireless devices to a central hub. 

Networks in SleepyNet:
 * Use a tree structure.
 * Are completely preconfigured (cannot be changed).
 * Support transmissions between all devices, but prioritize transmissions to parent ("hub") devices.
 * Prioritize power savings over latency.
 * Operate according to [this standard](./NetworkStandard.md).

SleepyNet fills the gap between hobbyist radio transmission libraries that don't offer features such as power management and radio portability (e.g. RadioHead), and enterprise-grade networking platforms with steep learning curves (e.g. OpenThread, emb6).

This page will be updated as the project progresses, however for now you can check out an outline of the network we intend to create [here](./NetworkStandard.md).
