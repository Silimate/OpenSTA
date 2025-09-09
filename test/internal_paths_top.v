module internal_paths_top (input in, input clk, output out);

  wire w1, w2, w3, w4, w5;

  internal_paths_cell t1 (.in(in), .clk(clk), .out(w1));

  BUFx2_ASAP7_75t_R b1 (.A(w1), .Y(w2));
  BUFx2_ASAP7_75t_R b2 (.A(w2), .Y(w3));

  internal_paths_cell t2 (.in(w2), .clk(clk), .out(out));

  internal_paths_cell t3 (.in(w3), .clk(clk), .out(out));

  BUFx2_ASAP7_75t_R b3 (.A(w3), .Y(w4));
  BUFx2_ASAP7_75t_R b4 (.A(w4), .Y(w5));

  internal_paths_cell t4 (.in(w4), .clk(clk), .out(out));

  internal_paths_cell t5 (.in(w5), .clk(clk), .out(out));
  
endmodule