import sys
from pathlib import Path
import datetime

class TB_Translator:
    def __init__(self, outfile):
        self.gentime = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        self.num_control_read = 0
        self.num_control_write = 0
        self.num_memory_read = 0
        self.num_memory_write = 0
        self.num_stall = 0
        self.num_ops = 0
        self.output = []
        self.outfile = outfile
    def _memory_read  (self, addr, data, pipeline):
        self.output += [f"assign tb_sequence[{self.num_ops}] = SEND_READ;"]
        self.output += [f"assign tb_read_addrs[{self.num_memory_read}] = {addr};"]
        self.output += [f"assign tb_read_datas_good[{self.num_memory_read}] = {data};"]
        self.output += [f"assign tb_read_pipeline[{self.num_memory_read}] = {pipeline};"]
        self.num_memory_read+=1
        self.num_ops+=1
    def memory_reads (self, addr, data):
        assert(len(addr) == len(data))
        for i in range(len(addr)):
            last = i == len(addr)-1
            pipeline = 0 if last else 1
            self._memory_read(addr[i], data[i], pipeline)
    def memory_read  (self, addr, data, len):
        for i in range(len):
            last = i == len-1
            pipeline = 0 if last else 1
            self._memory_read(addr, data, pipeline)
    def memory_write (self, addr, data, len):
        assert(len == 1)
        self.output += [f"assign tb_sequence[{self.num_ops}] = SEND_WRITE;"]
        self.output += [f"assign tb_write_addrs[{self.num_memory_write}] = {addr};"]
        self.output += [f"assign tb_write_datas[{self.num_memory_write}] = {data};"]
        self.num_memory_write+=1
        self.num_ops+=1
    def control_read (self, addr, data):
        self.output += [f"assign tb_sequence[{self.num_ops}] = SEND_TELEM_READ;"]
        self.output += [f"assign tb_telem_read_addrs[{self.num_control_read}] = {addr};"]
        self.output += [f"assign tb_telem_read_datas_good[{self.num_control_read}] = {data};"]
        self.num_control_read+=1
        self.num_ops+=1
    def control_write(self, addr, data):
        self.output += [f"assign tb_sequence[{self.num_ops}] = SEND_TELEM_WRITE;"]
        self.output += [f"assign tb_telem_write_addrs[{self.num_control_write}] = {addr};"]
        self.output += [f"assign tb_telem_write_datas[{self.num_control_write}] = {data};"]
        self.num_control_write+=1
        self.num_ops+=1
    def stall(self, stall_amt):
        self.output += [f"assign tb_sequence[{self.num_ops}] = STALL;"]
        self.output += [f"assign stall_amounts[{self.num_stall}] = {stall_amt};"]
        self.num_stall+=1
        self.num_ops+=1
    def stop(self):
        self.output += [f"assign tb_sequence[{self.num_ops}] = STOP;"]
        self.num_ops+=1
        self.do_output()
    def do_output(self):
        all_output = []
        all_output.append(f'initial $display("Testbench generated at {self.gentime}");')
        all_output.append(f"state_t tb_sequence[{self.num_ops}];")
        all_output.append(f"logic [63:0]stall_amounts[{self.num_stall}];")
        all_output.append(f"logic [mc_axi_if_pkg::MC_AXI_WAC_ADDR_BW-1:0] tb_write_addrs[{self.num_memory_write}];")
        all_output.append(f"logic [mc_axi_if_pkg::MC_AXI_WDC_DATA_BW-1:0] tb_write_datas[{self.num_memory_write}];")
        all_output.append(f"logic [mc_axi_if_pkg::MC_AXI_RAC_ADDR_BW-1:0] tb_read_addrs[{self.num_memory_read}];")
        all_output.append(f"logic [mc_axi_if_pkg::MC_AXI_RRC_DATA_BW-1:0] tb_read_datas_good[{self.num_memory_read}];")
        all_output.append(f"logic [0:0] tb_read_pipeline[{self.num_memory_read}];")
        all_output.append(f"logic [31:0] tb_telem_write_addrs[{self.num_control_write}];")
        all_output.append(f"logic [63:0] tb_telem_write_datas[{self.num_control_write}];")
        all_output.append(f"logic [31:0] tb_telem_read_addrs[{self.num_control_read}];")
        all_output.append(f"logic [63:0] tb_telem_read_datas_good[{self.num_control_read}];")

        all_output += self.output

        Path(self.outfile).write_text("\n".join(all_output))
        print(f"Generated testbench at time {self.gentime}")
