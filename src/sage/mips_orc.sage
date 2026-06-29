# SageMips ORC — Optimized Reference Counting (Sage)
# src/sage/mips_orc.sage

class MipsORC:
    proc init(self):
        self.orc_entries = []     # [{arc_idx, color, cycles}]
        self.allocs_since_scan = 0
        self.cycles_detected = 0
        self.objects_collected = 0
        self.enabled = true
        self.threshold = 256

    proc _mark_white(self):
        var i = 0
        while i < len(self.orc_entries):
            self.orc_entries[i]["color"] = 0  # WHITE
            i = i + 1

    proc _trial_delete(self, arc):
        var collected = 0
        var i = 0
        while i < len(self.orc_entries):
            let oe = self.orc_entries[i]
            if oe["color"] == 0:  # WHITE — potential cycle
                let aidx = oe["arc_idx"]
                if aidx >= 0 and aidx < len(arc.entries):
                    let ae = arc.entries[aidx]
                    if ae["refcount"] > 0:
                        ae["refcount"] = ae["refcount"] - 1
                        if ae["refcount"] <= 0:
                            ae["refcount"] = -1
                            arc.total_frees = arc.total_frees + 1
                            collected = collected + 1
                            oe["cycles"] = oe["cycles"] + 1
            i = i + 1
        if collected > 0:
            self.cycles_detected = self.cycles_detected + 1
            self.objects_collected = self.objects_collected + collected

    proc check_cycles(self, arc):
        if not self.enabled or not arc.enabled:
            return
        if self.allocs_since_scan < self.threshold:
            return
        self.allocs_since_scan = 0
        self._mark_white()
        self._trial_delete(arc)

    proc notify_alloc(self, arc):
        if not self.enabled:
            return
        self.allocs_since_scan = self.allocs_since_scan + 1

        # Register new ARC entries
        var i = 0
        while i < len(arc.entries):
            if arc.entries[i]["refcount"] <= 0:
                i = i + 1
                continue
            var found = false
            var j = 0
            while j < len(self.orc_entries):
                if self.orc_entries[j]["arc_idx"] == i:
                    found = true
                    break
                j = j + 1
            if not found:
                push(self.orc_entries, {"arc_idx": i, "color": 0, "cycles": 0})
            i = i + 1

        self.check_cycles(arc)

    proc stats(self):
        return [self.cycles_detected, self.objects_collected]
