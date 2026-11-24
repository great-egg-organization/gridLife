#include "automaticRerunSystem.h"

#ifdef INTERACTIVE_SIM
#include <iostream>
#include <GL/glew.h>
#include <GL/glut.h>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

void initApp() {
  sf::Window fakeWin;
  fakeWin.create(sf::VideoMode(sf::Vector2u(100, 100), 32), "FAKE");

  GLenum err = glewInit();
  if (err != GLEW_OK) {
    std::cout << glewGetErrorString(err) << "\n";
  }

  fakeWin.close();
}

void doWindowEvents(sf::RenderWindow &win, sf::Vector2i &lastMousePos) {
  std::optional<sf::Event> event = win.pollEvent();
  while (event.has_value()) {
    if (event.value().is<sf::Event::Closed>()) {
      win.close();
    }
    if (event.value().is<sf::Event::MouseWheelScrolled>()) {
      sf::Event::MouseWheelScrolled e = *event.value().getIf<sf::Event::MouseWheelScrolled>();

      sf::View view = win.getView();
      sf::Vector2f newSize = view.getSize();
      newSize *= std::exp(-e.delta / 10.0f);
      view.setSize(newSize);
      win.setView(view);
    }
    if (event.value().is<sf::Event::MouseMoved>()) {
      sf::Event::MouseMoved e = *event.value().getIf<sf::Event::MouseMoved>();

      if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        sf::View view = win.getView();
        sf::Vector2f newCenter = view.getCenter();
        sf::Vector2f diff = win.mapPixelToCoords(lastMousePos) - win.mapPixelToCoords(sf::Mouse::getPosition(win));
        newCenter += diff;
        view.setCenter(newCenter);
        win.setView(view);
      }

      lastMousePos = sf::Mouse::getPosition(win);
    }

    event = win.pollEvent();
  }
}

void interactiveSim() {
  sf::RenderWindow win;
  win.create(sf::VideoMode(sf::Vector2u(640, 640), 32), "BALLS");
  //win.setFramerateLimit(32);
  sf::Vector2i lastMousePos = sf::Mouse::getPosition(win);
  AutomaticRerunSystem simulation;

  {
    sf::View view = win.getView();
    view.setSize((sf::Vector2f)sc::BLOCK_DIM);
    view.setCenter(0.5f * (sf::Vector2f)sc::BLOCK_DIM);
    win.setView(view);
  }

  sf::Clock downtime;

  while (win.isOpen()) {
    doWindowEvents(win, lastMousePos);

    win.clear();
    simulation.simulator->render(&win);
    win.display();

    simulation.step();
  }
}

int main() { interactiveSim(); }

#else
void performanceTest() {
  AutomaticRerunSystem sim;
  sf::Clock clock;
  
  sim.go();
}

int main() { performanceTest(); }
#endif
