
from pathlib import Path
import time, pandas as pd
import numpy as np
from instruments.keithley2450 import Keithley2450
from instruments.dmm6500 import DMM6500
from report.excel_writer import write_efficiency_report

def run_efficiency(plan, out_dir: Path, prog):
    ins=plan["instruments"]; lim=plan["limits"]; sw=plan["sweep"]; outp=plan["outputs"]

    vin = Keithley2450(ins["keithley2450_1"]).as_voltage_source(curr_limit=lim["current_limit"])
    ch1 = Keithley2450(ins["keithley2450_2"]).as_current_sink(volt_limit=lim["voltage_limit_ch1"])
    x23 = Keithley2450(ins["keithley2450_3"]).as_current_sink(volt_limit=lim["voltage_limit_x23"])
    dmm2= DMM6500(ins["dmm6500_1"]).as_dcv()
    dmm3= DMM6500(ins["dmm6500_2"]).as_dcv()

    rows=[]; 
    total=max(1,len(sw["vin"])*len(sw["i_ch1"])*len(sw["i23"]));
    step=0    

    vin_cfg = sw["vin"]
    if isinstance(vin_cfg, dict):
        vin_list = np.arange(vin_cfg["start"], vin_cfg["stop"] + 1e-9, vin_cfg["step"]).round(3).tolist()
    else:
        vin_list =vin_cfg

    i1_cfg = sw["i_ch1"]
    if isinstance(i1_cfg, dict):
        i1_list = np.arange(i1_cfg["start"], i1_cfg["stop"] + 1e-9, i1_cfg["step"]).round(3).tolist()
    else:
        i1_list =i1_cfg  

    i23_cfg = sw["i23"]
    if isinstance(i23_cfg, dict):
        i23_list = np.arange(i23_cfg["start"], i23_cfg["stop"] + 1e-9, i23_cfg["step"]).round(3).tolist()
    else:
        i23_list =i23_cfg  

    total=max(1,len(vin_list)*len(i1_list)*len(i23_list));
    prog.event({"type":"artifact","total":total})

    try:
        for v in vin_list:
            vin.set_voltage(v)
            for i1 in i1_list:
                time.sleep(.1)
                ch1.set_sink_current(-i1)
                for i23 in i23_list:
                    time.sleep(.1)
                    x23.set_sink_current(-i23)
                    time.sleep(.1)
                    iin=vin.meas_current(); v1=ch1.meas_voltage(); v2=dmm2.meas_voltage(); v3=dmm3.meas_voltage(); v23=x23.meas_voltage()
                    p_in=v*iin; p_ch1=v1*i1; p_x=i23*(v2-v3); eta   = 0.0 if p_in <= 1e-9 else (p_ch1 + p_x) / p_in
                    eta = max(0.0, min(1.2, eta))
                    rows.append(dict(vin=v,i_ch1=i1,i23=i23,iin=iin,v1=v1,v2=v2,v3=v3,v23=v23, p_in=p_in,p_ch1=p_ch1,p_x=p_x,p_out=p_ch1+p_x,efficiency=eta))

                    step+=1; prog.update(state="running", percent=int(step/total*100), current={"vin":v,"i_ch1":i1,"i23":i23}, message=f"VIN={v} I1={i1} I23={i23}")
                    if plan["modes"] == "run" :
                        prog.event({"type":"kpi","name":"efficiency","vin":v,"i_ch1":i1,"i23":i23,"value":eta})
    finally:
        vin.off(); ch1.off(); x23.off(); dmm2.close(); dmm3.close()

    df=pd.DataFrame(rows)
    csvp=out_dir/outp["csv"]; df.to_csv(csvp, index=False)
    xlsxp=out_dir/outp["excel"]; write_efficiency_report(df, xlsxp)
    if plan["modes"] == "run" :
        prog.event({"type":"artifact","path":str(csvp)}); prog.event({"type":"artifact","path":str(xlsxp)})
