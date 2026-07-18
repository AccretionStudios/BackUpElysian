#include "Engine.h"

int main() {
    Elysian::Engine engine;
    
    // Window dimensions and title are now passed in here
    engine.Init(1600, 900, "Elysian-Editor");
    engine.Run(); 
    engine.Shutdown();

    return 0;
}