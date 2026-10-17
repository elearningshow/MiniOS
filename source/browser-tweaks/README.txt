MiniOS Nightly / Firefox low-memory tweaks (BROWSER-PERF-AP-01)
==============================================================

THIS IS OPTIONAL AND REVERSIBLE.
It is NOT part of the core MiniOS image. Do not bake it into minios.img.

Purpose
-------
Temporary relief for typing lag on heavy sites (especially Grok.com) on the
Gateway MA3 (~441 MB RAM + zram + 512 MB disk swap). The RAM upgrade is still
the main long-term fix. These preferences only reduce memory pressure.

How to apply
------------
1. Close Nightly / Firefox completely.
2. Copy this browser-tweaks folder onto the USB (or any persistent location).
3. In Terminal:
     cd /path/to/browser-tweaks
     ./apply-lowmem.sh
4. Start Nightly / Firefox again.
5. Type in the Grok.com search field and compare lag to before.

How to restore
--------------
Close the browser, then:
     ./restore-defaults.sh
Or delete user.js in the profile folder, or reset each preference in about:config
(right-click the preference → Reset).

Manual about:config fallback
----------------------------
If the scripts cannot find a profile:
1. Open Nightly / Firefox.
2. In the address bar type: about:config
3. Accept the warning.
4. Search for each name below and set the value:

   browser.sessionhistory.max_entries          5
   browser.sessionhistory.max_total_viewers    0
   javascript.options.mem.high_water_mark      64
   image.mem.max_decoded_image_kb              16384
   browser.cache.memory.capacity               16384
   browser.cache.memory.enable                 true
   dom.ipc.processCount                        1

Do not set browser.tabs.remote.autostart unless you are ready to test e10s off.
That preference is listed in the plan table but is not in nightly-lowmem.js.

Notes
-----
- Flash target remains MiniOS v0.2.6 (embedded Firefox ESR). These files apply
  to that profile too (minios.default).
- After more RAM is installed, run restore-defaults.sh or relax the values.
- Expectation: typing should feel better, not as fast as a 2 GB machine.
