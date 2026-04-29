# Installing the K Framework for Aria v0.18.x

The Aria repository does not vendor K. The local environment should provide:

- Java 17+ (`java -version`)
- `kompile`
- `krun`
- `kprove` for later proof slices
- Z3 4.12.1+ for symbolic/proof work

## Current workspace probe

As of this v0.18.0 seed:

- Java is available (`openjdk 21`).
- Z3 is available (`4.16.0` in the Python environment).
- Native `kompile`, `krun`, and `kprove` are not installed.
- Docker is installed but the current user cannot access `/var/run/docker.sock`.
- Ubuntu apt does not expose a direct `kframework` package here.

## Recommended install path

Use Runtime Verification's `kup` installer in user space:

```bash
bash <(curl -fsSL https://kframework.org/install)
kup install k
```

Then open a fresh shell or source the profile file that the installer prints,
and verify:

```bash
kompile --version
krun --version
kprove --version
```

## Docker fallback

If Docker access is enabled for the user, a K image can be used instead. The
runner in this directory intentionally targets native `kompile`/`krun` first;
container wrapping can be added once the exact image and mount conventions are
selected for CI.

## CI behavior

`run_k_tests.sh` exits with code `77` when K tools are missing. The CTest hook
uses that as a skip code so ordinary Aria compiler builds remain green while
formal-semantics infrastructure is being installed.