C++ Algorithmic Strategy Backtester

![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg) ![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)![C++ Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)

A high-performance, event-driven backtesting framework in C++ for quantitative trading strategies, optimized for high-frequency data(Market By Order feeds). Designed to simulate order book dynamics with microsecond precision, this tool is ideal for developing and testing HFT/quant strategies in the Chicago trading ecosystem.

Required installed dependencies on your machine
CMake
sudo apt install cmake

OpenSSL 
sudo apt install libssl-dev

zstd library
sudo apt install libzstd-dev

Built with g++

Unrealized gain calculated as liquidiation at global best ask (for long positions) and global
best bid (for short positions). Currently not simulating liquidation of large positions for unrealized gain calculations and only considering level 1 of the orderbook. 