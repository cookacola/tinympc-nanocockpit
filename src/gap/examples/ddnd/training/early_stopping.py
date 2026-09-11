"""Validation plateau stopping; independent from absolute-best checkpoint selection."""
import math
class PlateauStopper:
    def __init__(self, patience=5, relative_delta=.005, min_epochs=10):
        self.patience=patience; self.relative_delta=relative_delta; self.min_epochs=min_epochs
        self.reference=None; self.bad_epochs=0
    def update(self, value, completed_epochs):
        if not math.isfinite(value): raise ValueError("Non-finite validation loss")
        if self.reference is None or value < self.reference-self.relative_delta*max(abs(self.reference),1e-12):
            self.reference=value; self.bad_epochs=0
        else: self.bad_epochs+=1
        return completed_epochs>=self.min_epochs and self.bad_epochs>=self.patience
    def state_dict(self):
        return dict(reference=self.reference,bad_epochs=self.bad_epochs)
    def load_state_dict(self,state):
        self.reference=state["reference"]; self.bad_epochs=state["bad_epochs"]
