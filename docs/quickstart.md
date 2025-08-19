
## Installation
Honey does not yet provide a package or bundle to easily install it. 
Alternatively, you can download your desired version from releases, decompress it, and optionally add it to your `PATH` variable.

**Windows**

Unzip honey:
```powershell
Expand-Archive path/to/honey.zip -DestinationPath path/to/unzip
```

Adding the folder to your `PATH` variable cannot be done from the console (without some hacks), if you are not familar with how this is done,
I'd recommend googling `windows adding directory to path` and you should find plenty of guides on how to do it.

**Linux**

Unzip Honey:
```bash
tar -xf <honey>
```

And optionally add it to `PATH`:
```bash
# .zshrc
export PATH="$PATH:your/path/to/where/honey/exe/is/"
```

## Hello Hun!
Now to confirm if everything works, write this to `main.hun`:
```honey
main :: fn(|) i32 {
    println "hello hun!"
    0
}
```

And run `honeyc run main.hun`. If you get `hello hun!`, everything is working correctly!

## What's next?
Checkout the [language reference](./lang_reference.md) to see all working and planned features Honey has to offer.
