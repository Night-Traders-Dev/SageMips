# SageMips ARC — Automatic Reference Counting (Sage)
# src/sage/mips_arc.sage

class MipsARC:
    proc init(self):
        self.entries = []     # [{offset, len, refcount}]
        self.total_allocs = 0
        self.total_frees = 0
        self.enabled = true

    proc track(self, str_offset, str_len):
        if not self.enabled:
            return -1
        # Check if already tracked
        var i = 0
        while i < len(self.entries):
            if self.entries[i]["offset"] == str_offset and self.entries[i]["refcount"] >= 0:
                self.entries[i]["refcount"] = self.entries[i]["refcount"] + 1
                self.total_allocs = self.total_allocs + 1
                return i
            i = i + 1
        push(self.entries, {"offset": str_offset, "len": str_len, "refcount": 1})
        self.total_allocs = self.total_allocs + 1
        return len(self.entries) - 1

    proc retain(self, entry_idx):
        if not self.enabled or entry_idx < 0 or entry_idx >= len(self.entries):
            return
        if self.entries[entry_idx]["refcount"] > 0:
            self.entries[entry_idx]["refcount"] = self.entries[entry_idx]["refcount"] + 1

    proc release(self, entry_idx):
        if not self.enabled or entry_idx < 0 or entry_idx >= len(self.entries):
            return
        let e = self.entries[entry_idx]
        if e["refcount"] <= 0:
            return
        e["refcount"] = e["refcount"] - 1
        if e["refcount"] == 0:
            self.total_frees = self.total_frees + 1
            e["refcount"] = -1

    proc stats(self):
        var live = 0
        var i = 0
        while i < len(self.entries):
            if self.entries[i]["refcount"] > 0:
                live = live + 1
            i = i + 1
        return [self.total_allocs, self.total_frees, live]
