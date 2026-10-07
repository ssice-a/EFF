if (args.Length > 0 && args[0] == "--prepare-static-sources")
{
    try { return StaticModSourceCommand.Run(args); }
    catch (Exception error) { Console.Error.WriteLine("STATIC-SOURCES-REJECT reason=" + error.Message); return 1; }
}
Console.Error.WriteLine("eff_source_prepare --prepare-static-sources GAME INDEX MOD COMPILER");
return 1;
