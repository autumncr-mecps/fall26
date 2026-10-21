"""
This ended up being pretty different than what Jonathan and I were talking about in class,
because we ended up working alone after all. Instead of implementing a pheromone trail type
simulation, I went really heavy on probability in a way that lead to some pretty fun results.

"""

import pygame
import random
import util
import time
import math

# Initialize Pygame
pygame.init()

font = pygame.font.SysFont("monospace", 15)

# Screen dimensions
screen_width = 800
screen_height = 600
screen = pygame.display.set_mode((screen_width, screen_height))
pygame.display.set_caption("Ant Simulation")

food = []
anthill = []
ant_count = 0
ant_delivery = -1
min_food_found_time = -1
food_found_dir = []

# Colors
black = (0, 0, 0)
white = (255, 255, 255)

ANT_DIED = pygame.USEREVENT + 0
FOOD_DELIVERED = pygame.USEREVENT + 1

class Ant(pygame.sprite.Sprite):
    idx = 0
    state = 0
    birth = -1
    found_food = -1
    path = {}
    dir_probs = []
    color = []
    food_dir = []
    loc = []
    
    def __init__(self, x, y, idx, food_found_dir, food_delivered_count):
        super().__init__()
        self.loc = [x, y]
        # even after we've found food, chance of going in one of the normal directions
        # however, the more food has been delivered, the more likely it is that we'll go in that direction
        if len(food_found_dir) != 4 or random.randint(0, food_delivered_count) == 0:
            self.dir_probs = util.init_dir_probs[idx % len(util.init_dir_probs)][:]
        else:
            self.dir_probs = food_found_dir[:]
        self.idx = idx
        # give this ant a random color -- this also indirectly indicates its lifespan
        match food_delivered_count:
            # initial ants are all grey
            case 0:
                rint = random.randint(128, 210)
                self.color = [rint, rint, rint]
            # if we've found one piece of food, blue
            case 1:
                self.color = [random.randint(0, 32), random.randint(0, 32), random.randint(128, 210)]
            # if we've found two piece of food, purple
            case 2:
                self.color = [random.randint(128, 210), random.randint(0, 32), random.randint(128, 210)]
            # if we've found three pieces of food, green
            case 3:
                self.color = [random.randint(0, 32), random.randint(128, 210), random.randint(0, 32)]
            # after that, randomize
            case _:
                self.color = [random.randint(128, 210), random.randint(128, 210), random.randint(128, 210)]

        self.image = pygame.Surface((5, 5))
        self.image.fill(self.color)
        self.rect = self.image.get_rect(center=(x, y))
        self.birth = time.time()
        
    def update(self):
        global ant_delivery
        if self.state == 4:
            return

        # Move the ant
        if self.state == 0:
            # maybe die
            rint = random.randint(0, 9)
            if rint == 0:
                # ant movement gets a little more erratic as they get weak from hunger
                for i in range(4):
                    self.dir_probs[i] += random.randint(-2, 2)
                    self.dir_probs[i] = max(self.dir_probs[i], 0)
                    self.dir_probs[i] = min(self.dir_probs[i], 100)
                die = True
                for i in range(3):
                    self.color[i] = self.color[i] - 1
                    self.color[i] = max(self.color[i], 0)
                    if self.color[i] != 0:
                        die = False
                
                self.image.fill(self.color)
                if die:
                    self.state = 4
                    print("ANT", self.idx, "DIED")
                    pygame.event.post(pygame.event.Event(ANT_DIED))
                    return;

            # if we don't die, move
            moveDir(self)
            if checkFood(self.loc[0], self.loc[1]):
                print("ANT", self.idx, "FOUND FOOD")
                self.state = util.states["RETURNING"]
                self.food_dir = self.dir_probs[:]
                self.found_food_time = time.time() - self.birth
                return

            if self.rect.x >= screen_width - self.rect.width or self.rect.y >= screen_height - self.rect.height or self.rect.x <= 0 or self.rect.y <= 0:
                new_dir = random.randint(0, len(util.init_dir_probs) - 1)
                self.dir_probs = util.init_dir_probs[new_dir][:]
                for i in range(4):
                    self.dir_probs[i] += random.randint(-15, 15)

        elif self.state == 2:
            dist_x = anthill[0] - 10 - self.loc[0]
            dist_y = self.loc[1] - anthill[1] - 10
            total_dist = abs(dist_x) + abs(dist_y)

            if checkAnthill(self.loc[0], self.loc[1]) or total_dist == 0:
                self.state = util.states["STOP"]
                print("ANT", self.idx, "DELIVERED FOOD.")
                pygame.event.post(pygame.event.Event(FOOD_DELIVERED))
                ant_delivery = self
                return

            x_ratio = abs(dist_x) / total_dist;
            y_ratio = abs(dist_y) / total_dist;
            if dist_x > 0:
                self.dir_probs[1] = x_ratio * 100
                self.dir_probs[3] = 0
            elif dist_x < 0:
                self.dir_probs[3] = x_ratio * 100
                self.dir_probs[1] = 0
            if dist_y < 0:
                self.dir_probs[0] = y_ratio * 100
                self.dir_probs[2] = 0
            if dist_y > 0:
                self.dir_probs[2] = y_ratio * 100
                self.dir_probs[0] = 0

            moveDir(self)

def moveDir(self):
    dir = -1
    try_dirs = util.dir_dict.copy()
    for i in range(len(try_dirs)):
        try_rand_dir = random.randint(0, len(try_dirs) - 1)
        dir_key = list(try_dirs)[try_rand_dir]
        dir_val = list(try_dirs.values())[try_rand_dir]
        try_dirs.pop(dir_key)
        
        rint = random.randint(0, 100)
        if rint < self.dir_probs[dir_val]:
            dir = util.dirs[dir_val]

            self.loc[0] += dir[0]
            self.loc[1] += dir[1]

            self.rect.x = max(0, min(self.loc[0], screen_width - self.rect.width))
            self.rect.y = max(0, min(self.loc[1], screen_width - self.rect.width))

def checkFood(x, y):
    if x > (food[0] + 10) and x < (food[0] + 90) and y > (food[1] + 10) and y < (food[1] + 90):
        return 1
    return 0;


def checkAnthill(x, y):
    if x > (anthill[0] + 5) and x < (anthill[0] + 15) and y > (anthill[1] + 5) and y < (anthill[1] + 15):
        return 1
    return 0;

def updateLabel(ant_count, food_count, best_dir):
    str = f"Ant count: {ant_count}, food count: {food_count}"
    label = font.render(str, True, (0,0,0))
    screen.blit(label, (0, 0))

    str = f"Direction: {best_dir}"
    label = font.render(str, True, (0,0,0))
    screen.blit(label, (0, 20))

def main():
    global food, anthill, ant_delivery
    # randomize anthill location
    anthill = [random.randint(0, screen_width - 20), random.randint(0, screen_height - 20)]
    food = [random.randint(100, screen_width - 100), random.randint(100, screen_height - 100)]
    dist = math.dist(anthill, food)
    # food should always be at least 300px away from anthill
    while dist < 300:
        food = [random.randint(100, screen_width - 100), random.randint(100, screen_height - 100)]
        dist = math.dist(anthill, food)

    food_found_dir = []
    min_food_found_time = -1
    food_delivered_count = 0

    # Create ants
    ants = pygame.sprite.Group()
    ants.add(Ant(anthill[0], anthill[1], 0, food_found_dir, 0))
    ant_idx = 1
    ant_count = 1
    food_count = 19 # spawn up to 20 ants by default
    tics = 0
    
    screen.fill(white)
    
    # Game loop
    running = True
    clock = pygame.time.Clock()
    while running:
        tics = (tics + 1) % 20
        if tics == 0 and random.randint(0, 20) < food_count:
            print("ANT", ant_idx, "WAS BORN")
            ants.add(Ant(anthill[0], anthill[1], ant_idx, food_found_dir, food_delivered_count))
            ant_idx += 1
            ant_count += 1
            food_count -= 1

        if food_count == 0 and ant_count == 0:
            print("ALL ANTS HAVE DIED. TRAGEDY")
            pygame.event.post(pygame.event.Event(pygame.QUIT))
        
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == ANT_DIED:
                ant_count -= 1
            elif event.type == FOOD_DELIVERED:
                food_ant = ant_delivery
                if min_food_found_time < 0 or food_ant.found_food_time < min_food_found_time:
                    min_food_found_time = food_ant.found_food_time
                    food_found_dir = food_ant.food_dir
                    print("FOUND NEW FASTEST FOOD DIRECTION:", food_found_dir)
                food_delivered_count += 1
                food_count += 5
                ant_count -= 1

        # Update ants
        ants.update()

        # Draw everything
        ants.draw(screen)
        pygame.draw.ellipse(screen, (64, 50, 16), (anthill[0], anthill[1], 20, 20))
        pygame.draw.ellipse(screen, (16, 128, 32), (food[0], food[1], 100, 100))
        pygame.draw.rect(screen, (255, 255, 255), (0, 0, 300, 20))
        pygame.draw.rect(screen, (255, 255, 255), (0, 20, 300, 20))
        updateLabel(ant_count, food_count, food_found_dir)
        pygame.display.flip()
        clock.tick(120)

    pygame.quit()

main()
