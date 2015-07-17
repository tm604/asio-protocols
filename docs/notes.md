

    line_stream("\n"), "\x0D\x0A"

line_stream takes a char_stream. It collects characters until the linebreak, at which point it emits those characters.

    auto ls = line_stream(cs);


