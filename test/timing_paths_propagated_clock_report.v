module top (input in, input clk, output out);

  timing_cell t1 (.in(in), .out(out), .clk(clk));

endmodule
