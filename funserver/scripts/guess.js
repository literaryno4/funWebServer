let randomNum = Math.floor(Math.random() * 100) + 1;

const guesses = document.querySelector('.guesses');
const lastResult = document.querySelector('.lastResult');
const lowOrHi = document.querySelector('.lowOrHi');

const guessSubmit = document.querySelector('.guessSubmit');
const guessField = document.querySelector('.guessField');

let guessCount = 1;
let resetButton;
guessField.focus();

function reseGame() {
    guessCount = 1;

    const resetParas = document.querySelectorAll('.resultParas p');
    for (let i = 0; i < resetParas.length; i++) {
        resetParas[i].textContent = '';
    }

    guessField.disabled = false;
    guessSubmit.disabled = false;
    guessField.value = '';
    guessField.focus();

    lastResult.style.backgroundColor = 'white';

    randomNum = Math.floor(Math.random() * 100) + 1;
}

function setGameOver() {
    guessField.disabled = true;
    guessSubmit.disabled = true;
    resetButton = document.createElement('button');
    resetButton.textContent = 'New game';
    document.body.appendChild(resetButton);
    resetButton.addEventListener('click', reseGame);
}

function checkGuess() {
    let userGuess = Number(guessField.value);
    if (guessCount == 1) {
        guesses.textContent = 'Last guess: ';
    }
    guesses.textContent += userGuess + ' ';

    if (userGuess == randomNum) {
        lastResult.textContent = 'Right!';
        lastResult.style.backgroundColor = 'green';
        lowOrHi.textContent = '';
        setGameOver();
    } else if (guessCount == 10) {
        lastResult.textContent = '!!!!GAME OVER!!!!';
        setGameOver();
    } else {
        lastResult.textContent = 'Wrong!';
        lastResult.style.backgroundColor = 'red';
        if (userGuess < randomNum) {
            lowOrHi.textContent = 'Your guess is low';
        } else if (userGuess > randomNum) {
            lowOrHi.textContent = 'Your guess is high';
        }
    }

    guessCount++;
    guessField.value = '';
    guessField.focus();
}
guessSubmit.addEventListener('click', checkGuess);

