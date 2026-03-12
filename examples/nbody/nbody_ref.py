#!/usr/bin/env python3
"""
nbody_ref.py — Python reference for nbody.aria (EXP-3)

Exactly mimics the Aria simulation:
  N=10 bodies, dt=0.001, G=1.0, softening eps=0.1, 1000 steps.
  Symplectic Euler: update velocities then positions.

Initial conditions:
  body i: mass = 1.0 + i*0.1
           pos  = (i, 0, 0)
           vel  = (0, i*0.001, 0)

Output: total energy E = KE + PE after 1000 steps.
"""
import math

N = 10
dt = 0.001
G = 1.0
EPS = 0.1     # softening parameter (same as Aria code)
STEPS = 1000

# Initial conditions
px = [float(i) for i in range(N)]
py = [0.0] * N
pz = [0.0] * N
vx = [0.0] * N
vy = [i * 0.001 for i in range(N)]
vz = [0.0] * N
mass = [1.0 + i * 0.1 for i in range(N)]

for step in range(STEPS):
    # Zero accumulators
    accx = [0.0] * N
    accy = [0.0] * N
    accz = [0.0] * N

    # Pairwise forces
    for i in range(N):
        for j in range(i+1, N):
            dx = px[j] - px[i]
            dy = py[j] - py[i]
            dz = pz[j] - pz[i]
            dist2 = dx*dx + dy*dy + dz*dz + EPS*EPS
            dist  = math.sqrt(dist2)
            force = G * mass[i] * mass[j] / (dist2 * dist)
            fx = force * dx
            fy = force * dy
            fz = force * dz
            accx[i] += fx / mass[i]
            accy[i] += fy / mass[i]
            accz[i] += fz / mass[i]
            accx[j] -= fx / mass[j]
            accy[j] -= fy / mass[j]
            accz[j] -= fz / mass[j]

    # Symplectic Euler: velocity then position
    for i in range(N):
        vx[i] += accx[i] * dt
        vy[i] += accy[i] * dt
        vz[i] += accz[i] * dt
        px[i] += vx[i] * dt
        py[i] += vy[i] * dt
        pz[i] += vz[i] * dt

# Compute total energy (using same softening in PE)
KE = sum(0.5 * mass[i] * (vx[i]**2 + vy[i]**2 + vz[i]**2) for i in range(N))
PE = 0.0
for i in range(N):
    for j in range(i+1, N):
        dx = px[j] - px[i]
        dy = py[j] - py[i]
        dz = pz[j] - pz[i]
        dist = math.sqrt(dx*dx + dy*dy + dz*dz + EPS*EPS)
        PE -= G * mass[i] * mass[j] / dist

E = KE + PE
print(f"Python reference E = {E:.6f}")
print(f"Expected Aria output: E = {E:+.6f}")

# Dump final positions for detailed comparison
print("\nFinal positions (first 3 bodies):")
for i in range(3):
    print(f"  body {i}: ({px[i]:.6f}, {py[i]:.6f}, {pz[i]:.6f})")
