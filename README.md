# CS107e project

## Project title
Glove-controlled Tetris

## Team members
Nick Reisner, Sebastian Russo, Devon Smith

## Project description
In this project, we build a Tetris emulator with optimized gl in C. In addition to a fully-featured
Tetris game, we implement a glove peripheral for hand control of the falling block. We use 
gyroscope and acceleromator information through I2C to measure hand movement. 

## Member contribution
Nick:

I spearheaded the development of the tetris game without an outside reference. I developed the game using a PS/2 keyboard and allowed Sebastian/Devon to impliment the accelerometer. I will list my contributions in chronological order (following my GIT commits).

1. I built the game's foundation. This includes initializing the double frame buffer and figuring out how to use global constants (e.g. number of rows and columns of the playing field) to make the implimentation as smooth as possible. I continued by adding functions that would draw 1-by-1 blocks into particular x and y cordinates (cordinates corresponding to the game grid, not the framebuffer) and set up the armtimer and its interupts to drop a block. 

In this initial stage, I drew a background and had a single block fall until it reached the bottom.

2. I then malloced the 2-D array in which placed shapes are stored and set up the logic in the armtimer interrupt handler to place blocks into the 2-D array when the block was either at the bottom of the game grid or should stack ontop of a previously placed brick.

3. After Sebastian had set up the keyboard input and used my draw/clear block functions to drop and move the block side-to-side, implimented the clear row functionality. This had to clear the filled row visually, in the back-end 2-D array, and add to a score variable.

4. Using this score variable, I implimented the visual aspects of the score counter.

5. We realized the game was a bit too easy at this speed, so we wanted to speed it up! I implimented in the armtimer interrupt handler logic that would re-initialize the timer but faster.

At this point, we had the core foundation (blocks fall, can be moved, there is a score, etc.) needed to impliment 4-by-4 shapes rather than the 1-by-1 blocks.

6. After Sebastian figured out how to generate a random number (finding out about and integration rand.c) and created a first version of the shape struct, I implimented the remainder of shape.c. This started by creating a 4-D array that contained the 4-by-4 bitmap for the 4 rotational orientations of the 7 different shapes. I then used our previous draw/clear brick functions to iteratively draw/clear our 4-by-4 shapes. I also had to create a function to check for valid positions for a given shape (i.e. if the "on" bits in its bitmap are inbounds and don't overlap with a placed brick in the 2-D placed brick array. Finally, I  used Sebastian's work to fully build out the random shape generator, which required adapting the rand.c random number generator (using the system tick count as the seed for the random number instead of a fixed one so each game has a different block order).

7. Not very glamerous, but after building all of this out in shapes.c I had to figure out how to integrate multiple source files. Took me a while to find the issue, but I realized our header files only had #ifndef, but no #define!

8. I adapted the armtimer interrupt handler to use shapes, not 1-by-1 blocks. This meant swapping and adapting the draw, clear, check for valid position of, and check and clear row functionality.

9. I edited Sebastian's work on dropping and moving the block side-to-side with keyboard inputs to now use shapes. This also required swapping and adapting the draw, clear, check for valid position of, and check and clear row functionality. I then expanded keyboard inputs to rotate blocks and fast fall rather than instantly place at the bottom.

10. At this point, the core functionality was complete! I then added graphics and logic to create a start and loss screen. This required reorganizing our intializations so only the appropriate ones go off for each new game. I also had to create some helper graphics functions to center text and help me draw the blocky "TETRIS" on the homescreen.

11. Lastly, I added a highscore and a "next block" preview.

Sebastian:

1. I assisted Nick with a lot of the game mechanics of the system, though he took care of a lot of 
implementation. I also wrote many of the graphical elements/optimizing writing to buffes in mymodule.c. This is includes screen refresh, and block drawing/clearing, and their helper functions. I wrote these s.t. we did not even need to optimize the gl lib, becaause we were writing and swapping to both buffers in an efficient manner. I was responsible the move-left/move-right graphic, which required me to keep track of where the last block was (with some nasty edge cases, that eventually Nick, Maria, and I sorted out.) Finally, when I incorporated the sensor, I decomposed the armtimer into gravity() and sensor_poll(), and reconfigured the armtimer to poll the sensor 9/10 interrupts (using a global var), and call gravity 1/10 interrupts. 

2. The second main part of my contribution consisted of integrating our different modules together. I first integrated the keyboard with the game and the sensor with the game, which enabled me to develop movement left and right and the dropping block (and later the rotating block, which Nick wrote.) I assisted in ordering the initialization for the graphics, mechanics, interrupts, and later for the sensor.  I also wrote a significant amount of shapes.c. This starting the random number generator, which Nick later iterated on, writing the first iteration of the bitmaps, which Nick edited later for rotation, and the struct that represents shapes.  

3. I wrote the entire sensor.c file (except what I borrowed from the sensors/code/accel implementation) 
which included some nice math for keeping a running average of x,y, and z acceleration, recalibration, 
initial calibration, etc. I created the struct for to hold all of the values for the sensor implementation. You may notice that I chose to use lots of ringbuffers, mostly due to their FIFO functionality and convenient dequeue/enqeue. For the average running values, ringbuffers are an essential choice, but for immediately enqueueing and dequeuing values, I admit I could've used an array. I also tested and fine-tuned everything to the best of my ability (in the four days I was working on the sensor near the end of my project.) I soldered the sensor pins with Blake in lab
(finally learned how to solder.) After I was able to getting running values for the acceleration, I implemented a detection strategy with some controls -- global vars at the top of the file that change the sensitivity of the sensor. I got left and right hand motion detection working with ~80% accuracy, and then I tried both z-axis and y-axis movement detection, with less success. If I coudl've spent more time on the project, it would've been working to achieve better hand movement detection. Though without entirely redesigning the system, I felt I made a valiant attempt.

4. I also decomposed/commented a lot of stuff (sometimes while reading Nick's code) so I could understand it. One thing I want to get better at as I become a better developer is committing more frequently. My commits are usually a lot of code, because I feel I have to test a lot/comment before uploading. Really, though, this is bad practice (I'll work on it.)

Devon: 

1. I intially started working on implementing the BNO055 sensor for our group to gather data on the users movements. In doing this, I worked on understanding the I2C library and how the sensor actually worked. After working on this for about a week, we decided to scrap this sensor and transition to a new one. 

2. I sewed and soldered to create the hardware for the use of the sensor. 

3. I researched and learned how to use external sources like cloudcompute.com to convert audio files into wav files. I learned how to code wav to c files and how to adjust them to compress the audio being passed. In this project, I did this by converting the audio file of the tetris theme song into many different wav files

4. I integrated the audio with the game functionalily created by Sebastian and Nick to make it so that the song would play everytime the home screen came on to the monitor. 


## References
CS107e I2C protocol
CS107e Lectures/Sensors/Accel/Code/...

## Self-evaluation
Nick:

It was really cool to build Tetris without any reference and see how our decomposition at the start made our jobs 1000% easier at the end. I also really enjoyed the graphical aspect of this project. It was fun to see how functions could be optimized, how we could organize our code to be both more readable and functional, and its just fun to see the fruits of my labor so readily. The only thing I would do differently is consider at the start how we would want to decompose our work file-wise: we ran into some serious trouble with an overeliance on global variables that made our job tricky when implimenting other .c files from our primary one.

Overall, I'm super happy with how this went. It was a super fun process, and I think the end result (which is pretty much real Tetris) shows how much fun I had! 

Sebastian:

This was awesome. A really cool way to end the class, and it made me really excited about doing my own projects in the future (though likely not in bare metal C.) It was cool to be developing a lot of code alongside my partners and discussing development strategy. 

Thanks for an awesome quarter!

## Photos
Below is a link to some clips taken during development. We were unfortunately unable to get a video of the final version we brought to the project showcase. The two versions we brought to the project showcase (accelerometer and keyboard versions) are here on github. If you want to give our Tetris a try, the keyboard version should build with the files included in the "keyboard_version" directory.

https://youtu.be/lKJUElAGGsE
