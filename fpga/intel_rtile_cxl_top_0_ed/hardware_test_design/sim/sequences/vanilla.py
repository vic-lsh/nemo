import sys
from test_translator import TB_Translator

filter_start_addr = "32'b10_0000_0000_0000_1000"
filter_end_addr = "32'b10_0000_0000_0001_0000"
map_norm_shift_addr = "32'b10_0000_0000_0001_1000"
added_stall_addr = "32'b10_0000_0000_0000_0000"

invalid_control_resp = "{{60{1'b1}},4'b0000}"
undef = "64'hx"

filter_start_data = "64'h0000_0400"
filter_end_data = "64'h0000_1000"
map_norm_shift_data = "64'd10"
added_stall_data = "64'd10"

def vanilla_tb(tb: TB_Translator):
    tb.control_write(filter_start_addr,    filter_start_data)
    tb.control_write(filter_end_addr,      filter_end_data)
    tb.control_write(map_norm_shift_addr,  map_norm_shift_data)
    tb.control_write(added_stall_addr,     added_stall_data)

    tb.control_read(filter_start_addr,     filter_start_data)
    tb.control_read(filter_end_addr,       filter_end_data)
    tb.control_read(map_norm_shift_addr,   map_norm_shift_data)
    tb.control_read(added_stall_addr,      added_stall_data)

    # test that invalid address returns an error
    tb.control_read("32'b10_0000_0000_0010_0000", invalid_control_resp)

    # clear counters
    tb.control_read("0"        , undef)
    tb.control_read("32'h10008", undef)

    tb.memory_write("32'h0000_0400", "'hdeadb33f", 1)
    tb.memory_write("32'h0000_0840", "'hdeadb34f", 1)
    tb.memory_read ("32'h0000_0400", "'hdeadb33f", 1)

    # allow reads to propogate
    tb.stall("1000")

    # read telem outputs")
    tb.control_read("0",            "2")
    tb.control_read("32'h10008",    "1")
    tb.control_read("16",           undef)

    tb.stop()

if __name__ == "__main__":
    tb = TB_Translator(sys.argv[1])
    vanilla_tb(tb)
