// Headless setup + first report for the GP-100 V2.1 code blob (section0_code.bin, base 0x0).
// Forces Thumb mode, disassembles from the Cortex-M vector table, runs auto-analysis,
// then writes a report of references to selected strings. Read-only with respect to the firmware file.
//
// @category GP100
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import java.io.PrintWriter;
import java.math.BigInteger;
import java.util.LinkedHashSet;
import java.util.Set;

public class GP100Setup extends GhidraScript {
    @Override
    public void run() throws Exception {
        String outPath = getScriptArgs().length > 0 ? getScriptArgs()[0] : "/tmp/gp100_report.txt";
        Memory mem = currentProgram.getMemory();
        AddressSpace sp = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Register tmode = currentProgram.getLanguage().getRegister("TMode");
        Address min = mem.getMinAddress();
        Address max = mem.getMaxAddress();
        currentProgram.getProgramContext().setValue(tmode, min, max, BigInteger.ONE);

        // Cortex-M vector table at 0: word 0 = initial SP, words 1.. = handlers (bit 0 set = Thumb).
        int entries = 0;
        for (int i = 1; i < 48; i++) {
            long v = mem.getInt(sp.getAddress(i * 4L)) & 0xffffffffL;
            if ((v & 1) == 0 || v > max.getOffset()) continue;
            Address a = sp.getAddress(v & ~1L);
            disassemble(a);
            createFunction(a, i == 1 ? "Reset_Handler" : String.format("Vec_%02d", i));
            entries++;
        }
        analyzeAll(currentProgram);

        FunctionManager fm = currentProgram.getFunctionManager();
        try (PrintWriter out = new PrintWriter(outPath)) {
            out.printf("vector handlers created: %d%nfunctions after analysis: %d%n", entries, fm.getFunctionCount());

            DecompInterface di = new DecompInterface();
            di.openProgram(currentProgram);
            Set<Function> toDecompile = new LinkedHashSet<>();

            String[] needles = {
                "algorithmParaNameConstData.c", "algorType < MAX_ALGOR_NUM", "Drivers\\audio\\audio.c",
                "Studio Comp", "Cry Wah+", "GROUP_DATA"
            };
            for (String n : needles) {
                out.printf("%n== string: %s%n", n);
                Address from = min;
                int found = 0;
                while (from != null && found < 6) {
                    Address hit = mem.findBytes(from, n.getBytes("US-ASCII"), null, true, monitor);
                    if (hit == null) break;
                    found++;
                    out.printf("  at %s%n", hit);
                    int refs = 0;
                    for (Reference r : getReferencesTo(hit)) {
                        Function f = fm.getFunctionContaining(r.getFromAddress());
                        out.printf("    ref from %s (%s) in %s%n", r.getFromAddress(), r.getReferenceType(),
                            f == null ? "no function" : f.getName() + "@" + f.getEntryPoint());
                        if (f != null && toDecompile.size() < 12) toDecompile.add(f);
                        refs++;
                    }
                    if (refs == 0) out.println("    (no references found)");
                    from = hit.add(1);
                }
                if (found == 0) out.println("  not found");
            }

            for (Function f : toDecompile) {
                out.printf("%n==== decompile %s @ %s ====%n", f.getName(), f.getEntryPoint());
                DecompileResults r = di.decompileFunction(f, 60, monitor);
                out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "decompile failed: " + r.getErrorMessage());
            }
        }
        println("report written to " + outPath);
    }
}
