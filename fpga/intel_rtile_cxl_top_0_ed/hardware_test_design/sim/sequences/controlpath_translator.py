import sys
from test_translator import TB_Translator

filter_start_addr = "32'b10_0000_0000_0000_1000"
filter_end_addr = "32'b10_0000_0000_0001_0000"
map_norm_shift_addr = "32'b10_0000_0000_0001_1000"
added_stall_addr = "32'b10_0000_0000_0000_0000"

pipeline_selector_addr = "32'h40020"

invalid_control_resp = "{{60{1'b1}},4'b0000}"
undef = "64'hx"

filter_start_data = "64'h0000_0400"
filter_end_data = "64'h0000_1000"
map_norm_shift_data = "64'd10"
added_stall_data = "64'd10"

def set_pipeline_config(tb: TB_Translator, addr, data):
    tb.control_write(pipeline_selector_addr, addr)
    tb.control_write("0", data)

def check_pipeline_config(tb: TB_Translator, addr, data):
    tb.control_write(pipeline_selector_addr, addr)
    tb.control_read ("0", data)

def check_pipeline_telem(tb: TB_Translator, pipeline, sram_addr, data):
    tb.control_write(pipeline_selector_addr, pipeline)
    tb.control_read(sram_addr, data)

def controlpath_tb(tb: TB_Translator):
    set_pipeline_config(tb,   filter_start_addr,    filter_start_data)
    set_pipeline_config(tb,   filter_end_addr,      filter_end_data)
    set_pipeline_config(tb,   map_norm_shift_addr,  map_norm_shift_data)
    set_pipeline_config(tb,   added_stall_addr,     added_stall_data)

    check_pipeline_config(tb, filter_start_addr,    filter_start_data)
    check_pipeline_config(tb, filter_end_addr,      filter_end_data)
    check_pipeline_config(tb, map_norm_shift_addr,  map_norm_shift_data)
    check_pipeline_config(tb, added_stall_addr,     added_stall_data)

    # clear counters
    check_pipeline_telem(tb, "0", "0", undef)
    check_pipeline_telem(tb, "32'h10000", "8", undef)

    tb.memory_write("32'h0000_0400", "'hdeadb33f", 1)
    tb.memory_write("32'h0000_0840", "'hdeadb34f", 1)
    tb.memory_read ("32'h0000_0400", "'hdeadb33f", 1)

    # allow reads to propogate
    tb.stall("1000")

    # read telem outputs")
    check_pipeline_telem(tb, "0", "0", "2")
    check_pipeline_telem(tb, "32'h10000", "8", "1")
    check_pipeline_telem(tb, "0", "16", undef)

    tb.stop()

if __name__ == "__main__":
    tb = TB_Translator(sys.argv[1])
    controlpath_tb(tb)
