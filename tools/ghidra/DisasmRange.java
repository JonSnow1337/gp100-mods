// Print disassembly for a range. Usage: -postScript DisasmRange.java OUT START END   (read-only listing)
// @category GP100
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import java.io.PrintWriter;
public class DisasmRange extends GhidraScript {
    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        Address s = toAddr(a[1]), e = toAddr(a[2]);
        try (PrintWriter out = new PrintWriter(a[0])) {
            Instruction ins = getInstructionAt(s);
            if (ins == null) { disassemble(s); ins = getInstructionAt(s); }
            while (ins != null && ins.getAddress().compareTo(e) <= 0) {
                out.printf("%s  %s%n", ins.getAddress(), ins);
                ins = ins.getNext();
            }
        }
    }
}
