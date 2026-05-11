import codecs
with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Manager.h', 'r', 'utf-8-sig') as f:
    text = f.read()

if '#include "Renderer.h"' not in text:
    text = text.replace('class Scene;', '#include "Renderer.h"\n\nclass Scene;')
    with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Manager.h', 'w', 'utf-8-sig') as f:
        f.write(text)
