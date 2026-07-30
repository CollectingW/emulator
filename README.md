# Citron Neo — Nextendo Network / NZ:P Edition

A fork of the [Citron](https://git.citron-emu.org/citron/emu) Nintendo Switch emulator with two
purposes:

1. **Nextendo Network online play** — connect a Nextendo Network account and play supported titles
   online from Citron, without hosts-file edits, external DNS, or manual SSL bypass.
2. **Nazi Zombies: Portable (Switch Emulator Edition)** — the emulator-side fixes this fork was
   originally created for, and still carries.

> [!WARNING]
> **This is a work in progress. Expect bugs.**
>
> Online support is new, incomplete, and only lightly tested — largely by one person, on one
> machine, against one game. Things will break. If you hit a problem, please
> **[open an issue](../../issues)** and include:
>
> - your `citron_log.txt` (Linux: `~/.local/share/citron/log/citron_log.txt`)
> - the exact error code the game showed, if any (e.g. `2306-0802`)
> - the game, its version, and what you were doing when it failed
>
> A log makes the difference between a fixable report and a guess. For network problems, set the
> log filter to `*:Info Service:Debug Service.SSL:Debug WebService:Debug` before reproducing.

---

## Status

Verified working (Mario Kart 8 Deluxe 3.0.5, Linux/Vulkan):

| | |
| --- | --- |
| Account sign-in | Browser-based, OAuth loopback + PKCE — the emulator never sees your password |
| Hostname redirection | Nintendo online hostnames resolve to the configured Nextendo servers |
| TLS | Handshake with recovered SNI, ALPN pinned to HTTP/1.1 |
| Auth + secure server | Kerberos ticket, matchmaking, session entry |
| NAT check | Both vantage points, sub-second |
| Peer-to-peer | Hole-punching, races completed |
| Play-time sync | Pushed to your Nextendo profile on game exit |
| Presence | Published on sign-in and game start/stop |
| Profile name | Local Switch profile renamed to your account nickname |

Implemented but **not** verified — these need a second account on a second machine, which the author
does not have:

- friends appearing inside a game (`nn::friends` list and presence relay)
- a friend seeing *your* presence
- friend-hosted sessions / friend races

Not supported: Splatoon 2 and Super Smash Bros. Ultimate. Splatoon 2 verifies the account token
signature against the server's published key set, which needs a signing key this fork does not
have; Smash is untested. Only Mario Kart 8 Deluxe has been exercised.

## Setup

1. Build as you would upstream Citron (see `docs/`), or use a release build.
2. **Emulation → Configure → Network → Nextendo Network Integration**
   - tick **Enable Nextendo Network Redirection**
   - fill in **Nextendo Server IP** and **Nextendo NAT Server IP** (these must be two *distinct*
     addresses — the NAT check compares what two vantage points observe)
   - click **Sign In with Browser** and complete sign-in on the Nextendo site
3. Launch a supported game and enter its online mode.

No server addresses are compiled in. Unconfigured builds fall back to loopback and behave like
stock Citron, so redirection never happens by accident. `NEXTENDO_SERVER_IP`, `NEXTENDO_NAT_IP` and
`NEXTENDO_API` are honoured as environment overrides; the API override only accepts loopback or
HTTPS on the Nextendo domain, because those requests carry your account token.

**Friends** live in the same panel (**Network → Friends**) and under **Multiplayer → Nextendo
Friends** — add by friend code, accept or decline requests, see who is online.

> [!CAUTION]
> Your Network ID (PID) is effectively a credential on this network: the service accepts a bare PID
> as an identity. This fork deliberately never displays or logs it. Don't paste it anywhere, and
> don't ship `nextendo_account.txt` — it holds your session token — inside a build or archive.

## Credits and how this was built

The emulator is [Citron](https://git.citron-emu.org/citron/emu), itself derived from
[yuzu](https://github.com/yuzu-emu/yuzu). All emulation — CPU, GPU, audio, input, filesystem — is
theirs. This fork's changes are confined to the networking and account layers plus the surrounding
UI.

The Nextendo Network client behaviour was worked out by **studying the reference implementation**,
[Ryujinx-Nextendo](https://github.com/NextendoNetwork/Ryujinx-Nextendo), together with the
[published server sources](https://github.com/NextendoNetwork) — which document the protocol, the
endpoints, and the reasons behind a number of non-obvious decisions far better than black-box
guessing ever would. Credit where it is due: several fixes here exist because their comments
explained *why* something was necessary.

**No code from that project is copied into this one.** It could not be: it is licensed under
PolyForm Shield 1.0.0, which is incompatible with Citron's GPL. Everything here is an independent
C++ implementation written against Citron's own IPC, socket, TLS and configuration layers, which
differ substantially from Ryujinx's. Where the two diverge, it is deliberate:

- **Presence status is floored at *Online* while a game is running.** Titles publish *Offline* mid-
  session (Mario Kart does, on leaving the online menu); relaying that verbatim reports you offline
  while you are playing.
- **Redirection is gated behind a setting that defaults to off**, and no addresses are compiled in.
- Several fixes address Citron bugs that the reference never had to deal with — a mis-numbered
  `sfdnsres` command table, error codes decoded with their halves transposed, a `nifm` request that
  never left its pending state, ICMP errors from one peer tearing down a shared P2P socket, and
  play-time being lost on the normal shutdown path.

Also referenced: [Kinnay's NintendoClients](https://github.com/kinnay/NintendoClients) for NEX and
error-code documentation, and [switchbrew](https://switchbrew.org) for service definitions.

## Legal

Licensed **GPL-3.0-or-later**, as required by Citron. See [LICENSE](LICENSE).

This project ships no Nintendo code, keys, firmware or games, and is not affiliated with, endorsed
by, or associated with Nintendo. You must supply your own legally dumped games and system files,
exactly as with upstream Citron. "Nintendo Switch" and all game titles are trademarks of their
respective owners.

Nextendo Network is a community-run service, independent of this fork and of Nintendo.

<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>
<br>

# Abstract

Citron
is a specialized research software designed to orchestrate high-fidelity virtual environments. Unlike general-purpose tools, it focuses on the intersection of containerization and deep-system virtualization to provide researchers with granular control over network behavior and resource management.
Core Capabilities

    High-Fidelity Networking: Citron creates Docker container environments where network accuracy is the priority. It ensures that latency, throughput, and topology mirror real-world conditions with high precision, making it ideal for testing distributed systems or network protocols.
    Hybrid Orchestration: Beyond containers, it can spawn and manage multiple Virtual Machines (VMs) simultaneously. This allows for heterogeneous testing environments where containers and VMs interact seamlessly.
    Kernel Addressing: The software utilizes advanced kernel addressing techniques for resource management. By interacting closely with the host and guest kernels, Citron achieves low-overhead monitoring and advanced control over virtualized assets that standard management layers often miss.

Summary
Citron is a technical powerhouse for researchers who need more than just "standard" virtualization. It bridges the gap between the lightweight agility of Docker and the robust isolation of VMs, all while maintaining a rigorous, high-fidelity network layer managed through direct kernel-level operations.
Would you like to draft a README file or a technical abstract for a research paper based on this?

Citron functions as a
high-fidelity hardware-software co-design platform that bridges the gap between high-level container orchestration and low-level embedded hardware constraints. By integrating specialized architectures like Xtensa cores, Citron allows researchers to simulate and manage complex system-on-chip (SoC) behaviors within a virtualized framework.
1. Integration with Specialized Hardware (Xtensa Cores)
Citron addresses the unique constraints of Xtensa processors, which are modular, extensible 32-bit RISC architectures often used in networking and audio processing. 

    Custom Instruction Support: Citron can leverage the Tensilica Instruction Extension (TIE) language to simulate custom datapath elements and instructions within the virtualized environment.
    Protocol-Specific Optimization: Because Xtensa cores excel at processing packet headers and rule-based checks, Citron uses them to maintain network fidelity—ensuring that simulated network stacks perform with the same cycle-accurate behavior as physical networking chips. 

2. High-Fidelity Computing Devices
To achieve "high fidelity," Citron must manage the deterministic performance of computing devices, ensuring that virtualized sensors and actuators respond within real-world timing constraints. 

    Cycle-Accurate Modeling: It utilizes an Instruction Set Simulator (ISS) to provide instant feedback on how software interacts with the underlying hardware pipeline, preventing the "timing drift" common in standard VMs.
    Resource Determinism: Citron mitigates hardware constraints—such as limited memory and power—by tailoring the virtual environment to match the specific cache sizes and memory hierarchies of the target device. 

3. Advanced Kernel Addressing and Management
Citron’s specialization in kernel addressing allows it to bypass traditional virtualization overhead:

    Direct Register Access: By interacting with the Application Binary Interface (ABI), Citron manages how programs interact with the kernel, allowing for precise debugging and resource allocation across multiple spawned VMs.
    Kernel-Level Hypervisors: It functions similarly to a KVM (Kernel-based Virtual Machine), turning the host Linux system into a high-performance hypervisor that provides near-native execution of privileged instructions.
    Memory Lookup Interfaces: Citron can connect directly to arbitrary-width memories or RTL (Register Transfer Level) blocks for low-latency data transfers, effectively treating virtualized memory as if it were a direct point-to-point hardware connection. 

4. Technical Constraints & Architecture
Feature 	Implementation in Citron
Processor Type	Supports 32-bit configurable RISC/Xtensa architectures.
Virtualization Method	Hardware-assisted virtualization using extensions like Intel VT-x or AMD-V for direct execution.
Networking Layer	High-fidelity emulation that avoids standard I/O bottlenecks by using custom processor interfaces.
Management	Centralized synchronization controller for container scheduling and I/O compensation.
Would you like to explore how Citron handles specific network protocols like CoAP or MQTT on these virtualized Xtensa cores?
