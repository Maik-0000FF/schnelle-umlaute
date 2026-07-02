# Contributing

Thanks for your interest in improving Schnelle Umlaute. Contributions of all kinds are welcome, and one of the easiest and most valuable is improving the **character presets**.

## Improving a preset (native speakers welcome)

The bundled presets in [`addon/presets/`](addon/presets/) are plain text files, so improving one is quick and needs no build. If you are a **native speaker**, your help making your language's preset complete and correct for everyday writing is especially appreciated.

Each preset is one file, for example [`addon/presets/francais.txt`](addon/presets/francais.txt):

```
# Name: Français
# Description: Accents français (à â æ ç é è ê ë î ï ô œ ù û ü ÿ)
# Category: language

a=à,â,æ
c=ç
e=é,è,ê,ë
A=À,Â,Æ
C=Ç
E=É,È,Ê,Ë
```

### Format

- Three header lines: `# Name:` (shown in the library picker), `# Description:`, and `# Category:` (`language`, `symbols`, or `emoji`), then a blank line.
- One mapping per line: `key=variant1,variant2,...`.
  - The **key** is the single character you hold before the leader (Space). Uppercase letters are a **separate key** (`A=À,Â`), so add the uppercase forms too.
  - The **variants** after `=` are the outputs you cycle through, separated by commas, in the order you want to reach them (most common first).
- Only the first `=` is the separator, so an output may itself contain `=`.

### Escapes

- A literal **comma** in an output: double it. `h=Hello,, World` produces `Hello, World`.
- To use **`#`** or **`\`** as the input key (the comment marker and the escape character), prefix a backslash: `\#=...` maps `#`, and `\\=...` maps `\`.

See [Configuration](docs/CONFIGURATION.md) for the full mapping format, accent cycling, and snippets.

### Guidelines for language presets

- Cover the characters your language's **standard orthography** uses in native words, in both **lowercase and uppercase**.
- Order the variants by frequency, most common first.
- Keep to standard orthography; avoid loanword-only or purely stylistic diacritics unless they are part of everyday writing.

## Submitting a change

1. Fork the repository and create a branch.
2. Edit or add the preset file under `addon/presets/`.
3. Open a pull request against the **`dev`** branch, describing the language and what you changed. A source for the orthography is helpful.

## Other contributions

Bug reports and feature ideas are welcome as [issues](https://github.com/Maik-0000FF/schnelle-umlaute/issues). For code changes, please open a pull request against `dev`.
