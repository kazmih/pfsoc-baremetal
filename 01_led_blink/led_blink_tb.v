//----------------------------------------------------------------
// Testbench for led_blink
// Uses small CLK_FREQ to keep simulation fast
//----------------------------------------------------------------
`timescale 1ns/1ps

module tb_led_blink;

    reg  clk;
    reg  rst_n;
    wire led;

    // Small numbers so simulation finishes quickly
    led_blink #(
        .CLK_FREQ_HZ  (100),
        .BLINK_FREQ_HZ(1)
    ) dut (
        .clk   (clk),
        .rst_n (rst_n),
        .led   (led)
    );

    // 10ns clock period
    initial clk = 0;
    always #5 clk = ~clk;

    initial begin
        $dumpfile("led_blink.vcd");
        $dumpvars(0, tb_led_blink);

        rst_n = 0;
        #30;
        rst_n = 1;

        // Run long enough to see multiple toggles
        #2000;

        $display("Simulation complete. LED final state: %b", led);
        $finish;
    end

    // Print every time LED changes
    always @(led) begin
        $display("Time=%0t ns | LED toggled to: %b", $time, led);
    end

endmodule