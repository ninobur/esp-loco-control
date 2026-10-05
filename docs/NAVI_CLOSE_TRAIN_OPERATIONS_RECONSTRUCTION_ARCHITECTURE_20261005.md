# CTO and Circuit Express

## Architectural Understanding for NAVI Close Train Operations Reconstruction

## Scope and relationship to legacy CTO2

Legacy CTO2/LL-Auto and NAVI Close Train Operations are separate implementations. NAVI CTO does not extend, modify, or supersede the CTO2 implementation or protocol. It inherits selected proven behavioral concepts from CTO and Circuit Express, as documented in NAVI_CLOSE_TRAIN_OPERATIONS_RECONSTRUCTION_ARCHITECTURE_20261005.md, and reimplements those concepts using NAVI’s current capabilities, including DNA/MM position and IR movement measurement. Legacy CTO2 packet structures, protocol constraints, block logic, and implementation rules do not govern NAVI CTO unless explicitly adopted. Legacy CTO2 remains frozen independently.

### Coexistence boundary

Legacy CTO2 and NAVI CTO may coexist on the railway. Future NAVI CTO peer messaging must therefore be unambiguously distinguishable from legacy CTO2 peer traffic. Legacy CTO2 firmware must not interpret NAVI CTO packets as CtoPeerPacket traffic, and NAVI CTO must not interpret legacy CTO2 packets as NAVI peer-position messages. This is a compatibility requirement for future NAVI CTO protocol design, not authorization to modify the legacy CTO2 protocol now.

## Purpose

This document records the operating concepts behind the successful Lowline Close Train Operations (CTO) and Circuit Express (CE) systems that should guide reconstruction using NAVI.

The objective is not to port the old CTO code.

The objective is to preserve the operating ideas that made CTO and Circuit Express effective while replacing block-based inference, timing approximations, PWM profiles, and procedural remnants with NAVI’s much richer knowledge of physical position, measured movement, and locomotive-to-locomotive communication.

## 1. Original CTO concept

CTO allowed two independently powered locomotives to operate in close proximity without centralized moment-to-moment control.

The locomotives communicated directly using ESP-NOW.

The essential question each locomotive asked was relational:

> Where is the other locomotive relative to me?

The original system had only coarse block information, so this relationship had to be inferred from block reports.

Conceptually:

* If the other locomotive last reported the block I have just entered, it is ahead of me. I am the trailer.
* If the other locomotive’s reported position indicates that I am ahead, I am the leader.
* If neither condition exists, we currently have no close-train relationship.

Thus leader and trailer were roles derived from current railroad geometry, not permanent locomotive identities.

Toby could be leader on one encounter and trailer on another.

That principle remains fundamental.

## 2. The CTO pair

When two locomotives established a close-train relationship, they formed what we came to call a bubble.

The bubble was not a rigid formation.

The separation between the locomotives could expand and contract as they moved, stopped, and restarted. It behaved somewhat like a slinky.

The leader and trailer nevertheless remained operationally related.

The leader’s behavior affected the trailer, and the trailer’s arrival/release behavior affected the leader.

The bubble therefore represented a dynamic operating relationship, not merely physical proximity.

## 3. Station stop versus spacing stop

CTO revealed an important distinction that should be retained.

### Station stop

A station stop represents railroad service.

The train deliberately stops at a geographically defined location and dwells.

### Spacing stop

A spacing stop exists because another train occupies territory ahead.

Its purpose is maintaining the required relationship between trains.

Both may eventually command zero speed, but they arise from entirely different operating requirements.

They should remain conceptually distinct in NAVI.

## 4. Dispatcher authority

The dispatcher establishes the operating assignment.

The locomotive does not independently decide that it will become an Express, Local, skip a station, or change the railroad’s service pattern.

The authority hierarchy is:

```
Operator → Dispatcher → operating assignment
```

NAVI is analogous to the engineer.

The dispatcher gives the orders.

NAVI knows where the locomotive is and determines how to execute those orders.

The dispatcher therefore owns the operating overlay.

NAVI owns position and execution.

## 5. NAVI overlay model

The present NAVI architecture can be visualized as a row of Scrabble tiles.

Each MM interval is one tile.

An operating overlay is a complete set of tiles, with each tile defining the operating requirement applicable at that MM interval and direction.

Changing the overlay means replacing the set of tiles.

NAVI does not need to remember what the previous tiles said.

At any instant:

```
current MM interval + direction + current overlay → current operating requirement
```

A station procedure is therefore spatially encoded across a succession of MM intervals.

It appears sequential because the locomotive physically encounters the tiles sequentially.

The sequence exists primarily in the geography rather than in historical procedural memory.

Some local physical operations—such as the final stopping movement and dwell—may still require short-lived execution state because they occur at finer resolution than the MM map.

## 6. What NAVI changes about CTO

Original CTO had only coarse block position.

NAVI has:

* MM-level position;
* direction;
* surveyed MM spacing;
* Hall observations;
* IR-measured movement;
* IR speed;
* physical route topology;
* direct ESP-NOW communication.

Therefore NAVI should not reproduce CTO’s block machinery simply because CTO worked.

The old questions:

> “Is the other train in my block?”

and

> “Is the other train in the adjacent block?”

were proxies for a much better question NAVI can now answer:

> Where exactly is the other train relative to me?

The relationship survives.

The block inference does not.

## 7. A locomotive is a physical consist, not a point

A NAVI position represents the location of the Hall sensor, but the train occupies physical territory around that point.

For the currently defined consist:

* front of consist: approximately 450 mm ahead of the Hall sensor;
* rear of consist: approximately 1,200 mm behind the Hall sensor.

Thus peer NAVIs should exchange enough information to understand not merely the other locomotive’s Hall MM but the physical envelope occupied by its consist.

This allows a following NAVI to determine where the rear of the leading train actually is.

That provides the foundation for much more precise close-train operation.

## 8. NAVI-to-NAVI communication

NAVI A and NAVI B communicate their position information directly using ESP-NOW.

The dispatcher does not need to act as the real-time intermediary for train separation.

Each NAVI knows:

* its own position;
* its own direction;
* its own consist geometry;
* the peer’s reported position;
* the peer’s relevant operating information.

From this, the locomotives can determine their current relationship.

The dispatcher remains the source of operating intent.

The peer NAVIs provide the immediate physical intelligence necessary to execute that intent around another train.

## 9. Continuous following

The old CTO system often reduced the traffic problem to a block-based stop decision.

NAVI can do better.

Because the following locomotive knows the location and physical extent of the train ahead, it can progressively modify its own movement as the separation closes.

Therefore close-train operation need not consist simply of:

```
run → detect proximity → stop.
```

It can become:

```
run → close → progressively slow → follow → stop if required.
```

The stopping point behind another train is dynamic.

It is determined from the actual physical relationship between the two consists rather than from a fixed station or block stopping point.

## 10. Bubble

A consist describes the physical envelope of one train.

A bubble describes the dynamic operating relationship and combined envelope of two trains operating in close relationship.

Within a bubble:

* one train is currently ahead;
* one train is currently following;
* separation may increase or decrease;
* the following train responds to the geometry of the train ahead;
* leader/follower roles are relational rather than permanently assigned.

The bubble concept should remain useful with NAVI because it scales.

NGR should eventually be able to support:

* one two-train bubble;
* a third independent locomotive;
* potentially three locomotives interacting;
* two separate bubbles.

The architecture should therefore avoid assumptions that exactly two locomotive identities permanently constitute the railroad.

## 11. Circuit Express

Circuit Express is an operational profile, not a separate choreography system.

Its governing principle was:

> CE changes service profiles, not encounter choreography.

CE begins from an existing CTO relationship.

At CE initiation:

* the current CTO leader becomes Express;
* the current CTO trailer becomes Local.

These are temporary roles.

### Express

The Express:

* operates at the higher service speed;
* skips station stops;
* has no station dwell.

### Local

The Local:

* operates at the Local service speed;
* makes station stops;
* performs the applicable dwell.

The differing service patterns naturally alter the physical relationship between the trains.

The Express gains on the Local.

No centralized choreography is required to manufacture that convergence.

## 12. Circuit Express self-resolution

One of the most important CE concepts is that the divergence should self-resolve.

The trains begin with temporary Express and Local roles.

Their different service profiles change their physical relationship.

Eventually the Express catches the Local.

When the physical relationship again satisfies the requirements for close-train operation, ordinary paired operation can re-establish itself.

Critically, the former Local can naturally become the new leader and the former Express the trailer.

Thus:

> role reversal emerges from physical geometry.

It does not require the dispatcher to script the reversal.

This is a principle worth preserving in NAVI CTO.

## 13. Circuit Express as an overlay

In NAVI terms, CE is another operating overlay.

A normal-service overlay may define station stops for a locomotive.

The Express overlay substitutes a different set of MM instructions in which those stations are passed.

The Local overlay retains the applicable stopping instructions and Local service speed.

When CE ends, those overlays cease to have authority.

The locomotives immediately operate according to the replacement operating assignment.

No lingering Express or Local latch should survive the overlay change.

The historical CE bug in which the former Local retained its Local profile after CTO recapture demonstrates exactly why this rule matters.

## 14. What should not be carried forward from old CTO/CE

The following mechanisms solved limitations of the old block-based system and should not automatically be reproduced in NAVI:

* four-block position as primary location authority;
* same-block/adjacent-block inference when exact MM geometry is available;
* fixed delay-based stopping geometry;
* PWM values used as the operating language;
* stale-block recapture logic;
* `WAIT_LEADER` and similar states created by stale block reports;
* temporary trailing-candidate escape timers;
* two-block recapture-suppression windows;
* old RELEASE choreography where precise current geometry can provide the required relationship;
* historical role/profile latches after the physical or dispatcher relationship has changed.

The operational need behind any of these mechanisms should be preserved only if it still exists.

The old mechanism itself has no special authority.

## 15. What should be carried forward

The following concepts remain valuable:

1. Dispatcher owns operating intent.
2. NAVI owns position and locomotive execution.
3. Peer NAVIs communicate directly over ESP-NOW.
4. Each train is represented as a physical consist rather than a point.
5. Leader and follower are relationships derived from current geometry.
6. A bubble is a dynamic two-train operating relationship.
7. The following locomotive can progressively regulate speed relative to the train ahead.
8. Station stops and spacing stops are different operating requirements.
9. Circuit Express is temporary divergence of service profiles.
10. CE roles are temporary and relational.
11. CE convergence and leader/follower reversal should emerge naturally from train geometry.
12. Operating-profile changes replace the previous overlay immediately; obsolete role state does not retain authority.
13. The architecture should scale beyond exactly two locomotives—to a third locomotive and eventually multiple bubbles.

## 16. Guiding principle for NAVI CTO reconstruction

The original CTO system discovered useful railroad relationships despite having only coarse block information.

NAVI should preserve those relationships while replacing the mechanisms that existed only because precise position was unavailable.

The reconstruction principle is therefore:

> **Preserve the railroad logic. Replace the old inference machinery with NAVI’s actual knowledge of position, movement, consist geometry, and peer location.**

Or, in the language of the current NAVI architecture:

> **The dispatcher supplies the operating tiles. NAVI knows which tile it occupies. Peer NAVIs tell one another where their trains physically are. Each engineer then executes the current orders while respecting the actual geometry of the railroad and the other trains.**
