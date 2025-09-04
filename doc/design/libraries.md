# Libraries

| Name                     | Description |
|--------------------------|-------------|
| *libbitquantum_cli*         | RPC client functionality used by *bitquantum-cli* executable |
| *libbitquantum_common*      | Home for common functionality shared by different executables and libraries. Similar to *libbitquantum_util*, but higher-level (see [Dependencies](#dependencies)). |
| *libbitquantum_consensus*   | Consensus functionality used by *libbitquantum_node* and *libbitquantum_wallet*. |
| *libbitquantum_crypto*      | Hardware-optimized functions for data encryption, hashing, message authentication, and key derivation. |
| *libbitquantum_kernel*      | Consensus engine and support library used for validation by *libbitquantum_node*. |
| *libbitquantumqt*           | GUI functionality used by *bitquantum-qt* and *bitquantum-gui* executables. |
| *libbitquantum_ipc*         | IPC functionality used by *bitquantum-node*, *bitquantum-wallet*, *bitquantum-gui* executables to communicate when [`-DENABLE_IPC=ON`](multiprocess.md) is used. |
| *libbitquantum_node*        | P2P and RPC server functionality used by *bitquantumd* and *bitquantum-qt* executables. |
| *libbitquantum_util*        | Home for common functionality shared by different executables and libraries. Similar to *libbitquantum_common*, but lower-level (see [Dependencies](#dependencies)). |
| *libbitquantum_wallet*      | Wallet functionality used by *bitquantumd* and *bitquantum-wallet* executables. |
| *libbitquantum_wallet_tool* | Lower-level wallet functionality used by *bitquantum-wallet* executable. |
| *libbitquantum_zmq*         | [ZeroMQ](../zmq.md) functionality used by *bitquantumd* and *bitquantum-qt* executables. |

## Conventions

- Most libraries are internal libraries and have APIs which are completely unstable! There are few or no restrictions on backwards compatibility or rules about external dependencies. An exception is *libbitquantum_kernel*, which, at some future point, will have a documented external interface.

- Generally each library should have a corresponding source directory and namespace. Source code organization is a work in progress, so it is true that some namespaces are applied inconsistently, and if you look at [`add_library(bitquantum_* ...)`](../../src/CMakeLists.txt) lists you can see that many libraries pull in files from outside their source directory. But when working with libraries, it is good to follow a consistent pattern like:

  - *libbitquantum_node* code lives in `src/node/` in the `node::` namespace
  - *libbitquantum_wallet* code lives in `src/wallet/` in the `wallet::` namespace
  - *libbitquantum_ipc* code lives in `src/ipc/` in the `ipc::` namespace
  - *libbitquantum_util* code lives in `src/util/` in the `util::` namespace
  - *libbitquantum_consensus* code lives in `src/consensus/` in the `Consensus::` namespace

## Dependencies

- Libraries should minimize what other libraries they depend on, and only reference symbols following the arrows shown in the dependency graph below:

<table><tr><td>

```mermaid

%%{ init : { "flowchart" : { "curve" : "basis" }}}%%

graph TD;

bitquantum-cli[bitquantum-cli]-->libbitquantum_cli;

bitquantumd[bitquantumd]-->libbitquantum_node;
bitquantumd[bitquantumd]-->libbitquantum_wallet;

bitquantum-qt[bitquantum-qt]-->libbitquantum_node;
bitquantum-qt[bitquantum-qt]-->libbitquantumqt;
bitquantum-qt[bitquantum-qt]-->libbitquantum_wallet;

bitquantum-wallet[bitquantum-wallet]-->libbitquantum_wallet;
bitquantum-wallet[bitquantum-wallet]-->libbitquantum_wallet_tool;

libbitquantum_cli-->libbitquantum_util;
libbitquantum_cli-->libbitquantum_common;

libbitquantum_consensus-->libbitquantum_crypto;

libbitquantum_common-->libbitquantum_consensus;
libbitquantum_common-->libbitquantum_crypto;
libbitquantum_common-->libbitquantum_util;

libbitquantum_kernel-->libbitquantum_consensus;
libbitquantum_kernel-->libbitquantum_crypto;
libbitquantum_kernel-->libbitquantum_util;

libbitquantum_node-->libbitquantum_consensus;
libbitquantum_node-->libbitquantum_crypto;
libbitquantum_node-->libbitquantum_kernel;
libbitquantum_node-->libbitquantum_common;
libbitquantum_node-->libbitquantum_util;

libbitquantumqt-->libbitquantum_common;
libbitquantumqt-->libbitquantum_util;

libbitquantum_util-->libbitquantum_crypto;

libbitquantum_wallet-->libbitquantum_common;
libbitquantum_wallet-->libbitquantum_crypto;
libbitquantum_wallet-->libbitquantum_util;

libbitquantum_wallet_tool-->libbitquantum_wallet;
libbitquantum_wallet_tool-->libbitquantum_util;

classDef bold stroke-width:2px, font-weight:bold, font-size: smaller;
class bitquantum-qt,bitquantumd,bitquantum-cli,bitquantum-wallet bold
```
</td></tr><tr><td>

**Dependency graph**. Arrows show linker symbol dependencies. *Crypto* lib depends on nothing. *Util* lib is depended on by everything. *Kernel* lib depends only on consensus, crypto, and util.

</td></tr></table>

- The graph shows what _linker symbols_ (functions and variables) from each library other libraries can call and reference directly, but it is not a call graph. For example, there is no arrow connecting *libbitquantum_wallet* and *libbitquantum_node* libraries, because these libraries are intended to be modular and not depend on each other's internal implementation details. But wallet code is still able to call node code indirectly through the `interfaces::Chain` abstract class in [`interfaces/chain.h`](../../src/interfaces/chain.h) and node code calls wallet code through the `interfaces::ChainClient` and `interfaces::Chain::Notifications` abstract classes in the same file. In general, defining abstract classes in [`src/interfaces/`](../../src/interfaces/) can be a convenient way of avoiding unwanted direct dependencies or circular dependencies between libraries.

- *libbitquantum_crypto* should be a standalone dependency that any library can depend on, and it should not depend on any other libraries itself.

- *libbitquantum_consensus* should only depend on *libbitquantum_crypto*, and all other libraries besides *libbitquantum_crypto* should be allowed to depend on it.

- *libbitquantum_util* should be a standalone dependency that any library can depend on, and it should not depend on other libraries except *libbitquantum_crypto*. It provides basic utilities that fill in gaps in the C++ standard library and provide lightweight abstractions over platform-specific features. Since the util library is distributed with the kernel and is usable by kernel applications, it shouldn't contain functions that external code shouldn't call, like higher level code targeted at the node or wallet. (*libbitquantum_common* is a better place for higher level code, or code that is meant to be used by internal applications only.)

- *libbitquantum_common* is a home for miscellaneous shared code used by different Bitquantum Core applications. It should not depend on anything other than *libbitquantum_util*, *libbitquantum_consensus*, and *libbitquantum_crypto*.

- *libbitquantum_kernel* should only depend on *libbitquantum_util*, *libbitquantum_consensus*, and *libbitquantum_crypto*.

- The only thing that should depend on *libbitquantum_kernel* internally should be *libbitquantum_node*. GUI and wallet libraries *libbitquantumqt* and *libbitquantum_wallet* in particular should not depend on *libbitquantum_kernel* and the unneeded functionality it would pull in, like block validation. To the extent that GUI and wallet code need scripting and signing functionality, they should be able to get it from *libbitquantum_consensus*, *libbitquantum_common*, *libbitquantum_crypto*, and *libbitquantum_util*, instead of *libbitquantum_kernel*.

- GUI, node, and wallet code internal implementations should all be independent of each other, and the *libbitquantumqt*, *libbitquantum_node*, *libbitquantum_wallet* libraries should never reference each other's symbols. They should only call each other through [`src/interfaces/`](../../src/interfaces/) abstract interfaces.

## Work in progress

- Validation code is moving from *libbitquantum_node* to *libbitquantum_kernel* as part of [The libbitquantumkernel Project #27587](https://github.com/bitquantum/bitquantum/issues/27587)
