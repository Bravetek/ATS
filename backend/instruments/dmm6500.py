
import pyvisa

class DMM6500:
    def __init__(self, resource):
        self.rm = pyvisa.ResourceManager()
        self.dev = self.rm.open_resource(resource, timeout=8000)
	
    def as_dcv(self):
        d = self.dev
        d.write("*RST"); 
        d.write("STAT:CLE"); 
        return self
    def meas_voltage(self): return float(self.dev.query(":MEAS:VOLT:DC?"))
    def close(self): self.dev.close()
