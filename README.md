###### fireice-uk's and psychocrypt's
# rx-switcher: switch xmr-stak to xmr-stak-rx

This tool based on xmr-stak. 
The switcher is killing all `xmr-stak` instances on your system and starts automatically xmr-stak-rx as soon as the Monero network is forking to the POW randomX.

# How to

You can download rx-switcher precompiled along with [xmr-stak-rx](https://github.com/fireice-uk/xmr-stak/releases).
1. keep you normal `xmr-stak` miner running on Monero 
2. copy the files `config.txt` and `pools.txt` from `1.` to the folder where `rx-switcher` and `xmr-stak-rx` is located
3. do not copy `cpu.txt`, `nvidia.tx` or `amd.txt`, due to new memory requirements for randomX we suggest to generate new configs to avoid that the miner crashes on the first start.
4. run `rx-switcher`
5. as soon as the Monero network is forking `xmr-stak` will be stopped and `xmr-stak-rx` will automatically start
6. after the network fork we suggest to start `xmr-stak-rx` directly and not use `rx-switcher anymore

## Default Developer Donation

If you want to donate directly to support further development, here is my wallet

fireice-uk:
```
4581HhZkQHgZrZjKeCfCJxZff9E3xCgHGF25zABZz7oR71TnbbgiS7sK9jveE6Dx6uMs2LwszDuvQJgRZQotdpHt1fTdDhk
```

psychocrypt:
```
45tcqnJMgd3VqeTznNotiNj4G9PQoK67TGRiHyj6EYSZ31NUbAfs9XdiU5squmZb717iHJLxZv3KfEw8jCYGL5wa19yrVCn
```
