New command line interface
--------------------------

A new `bitquantum` command line tool has been added to make features more
discoverable and convenient to use. The `bitquantum` tool just calls other
executables and does not implement any functionality on its own. Specifically
`bitquantum node` is a synonym for `bitquantumd`, `bitquantum gui` is a synonym for
`bitquantum-qt`, and `bitquantum rpc` is a synonym for `bitquantum-cli -named`. Other
commands and options can be listed with `bitquantum help`. The new tool does not
replace other tools, so existing commands should continue working and there are
no plans to deprecate them.

Install changes
---------------

The `test_bitquantum` executable is now located in `libexec/` rather than `bin/`.
It can still be executed directly, or accessed through the new `bitquantum` command
line tool as `bitquantum test`.

Other executables which are only part of source releases and not built by
default: `test_bitquantum-qt`, `bench_bitquantum`, `bitquantum-chainstate`,
`bitquantum-node`, and `bitquantum-gui` are also now installed in `libexec/`
instead of `bin/` and can be accessed through the `bitquantum` command line tool.
See `bitquantum help` output for details.
