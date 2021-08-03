let myHeading = document.querySelector('h1');
myHeading.onclick = function () {
    alert('yahaha');
};

let myImage = document.querySelector('img');

myImage.onclick = function() {
    let mySrc = myImage.getAttribute('src');
    myImage.setAttribute('src', 'images/IMG_' + Math.floor(Math.random()*8) + ".jpeg");
};

let myButton = document.querySelector('button');

function setUserName() {
  let myName = prompt('Input your name: ');
  if (!myName || myName == null) {
    setUserName()
  } else {
    localStorage.setItem('name', myName);
    myHeading.textContent = 'Welcome，' + myName;
  }
}

if(!localStorage.getItem('name')) {
  setUserName();
} else {
  let storedName = localStorage.getItem('name');
  myHeading.textContent = 'Welcome，' + storedName;
}

myButton.onclick = function() {
   setUserName();
}