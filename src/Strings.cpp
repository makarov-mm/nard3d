#include "Strings.h"

#include <array>
#include <cstddef>
#include <utility>

namespace {

struct Entry {
    Str id;
    std::string_view english;
    std::string_view russian;
};

constexpr std::array<Entry, static_cast<std::size_t>(Str::Count)> Table{{
    {Str::Title, "3D Nardy", "Нарды 3D"},
    {Str::Game, "Game", "Игра"},
    {Str::GameLong, "Long nardy", "Длинные нарды"},
    {Str::GameShort, "Short nardy", "Короткие нарды"},
    {Str::Mode, "Mode", "Режим"},
    {Str::ModeComputer, "Vs computer", "С компьютером"},
    {Str::ModeTwoPlayers, "Two players", "Два игрока"},
    {Str::ModeDemo, "Demo game", "Демонстрация"},
    {Str::Level, "Level", "Уровень"},
    {Str::Level1, "Novice", "Новичок"},
    {Str::Level2, "Beginner", "Начинающий"},
    {Str::Level3, "Amateur", "Любитель"},
    {Str::Level4, "Strong", "Сильный"},
    {Str::Level5, "Master", "Мастер"},
    {Str::Theme, "Theme", "Оформление"},
    {Str::Sound, "Sound", "Звук"},
    {Str::LanguageName, "English", "Русский"},
    {Str::AutoRoll, "Auto roll", "Автобросок"},
    {Str::Camera, "View", "Вид"},
    {Str::CameraOverview, "overview", "обзор"},
    {Str::CameraTop, "from above", "сверху"},
    {Str::NewGame, "New game", "Новая игра"},
    {Str::Undo, "Undo", "Отменить"},
    {Str::Roll, "Roll", "Бросить кости"},
    {Str::Hint, "Hint", "Подсказка"},
    {Str::NameYou, "You", "Вы"},
    {Str::NameComputer, "Computer", "Компьютер"},
    {Str::NameWhite, "White", "Белые"},
    {Str::NameBlack, "Black", "Чёрные"},
    {Str::TurnYou, "Your turn", "Ваш ход"},
    {Str::TurnComputer, "Computer's turn", "Ход компьютера"},
    {Str::TurnWhite, "White to play", "Ход белых"},
    {Str::TurnBlack, "Black to play", "Ход чёрных"},
    {Str::WinYou, "You win!", "Вы победили!"},
    {Str::WinComputer, "The computer wins", "Победил компьютер"},
    {Str::WinWhite, "White wins", "Победа белых"},
    {Str::WinBlack, "Black wins", "Победа чёрных"},
    {Str::StartsYou, "You go first", "Вы начинаете"},
    {Str::StartsComputer, "The computer goes first", "Компьютер начинает"},
    {Str::StartsWhite, "White goes first", "Начинают белые"},
    {Str::StartsBlack, "Black goes first", "Начинают чёрные"},
    {Str::Opening, "Who goes first", "Кто начинает"},
    {Str::OpeningDetail, "Each player throws one die", "Каждый бросает одну кость"},
    {Str::OpeningTie, "A tie, {}:{} — throwing again", "Поровну, {}:{} — бросаем снова"},
    {Str::RollPrompt, "Roll: Space or click the board", "Бросок: пробел или щелчок по доске"},
    {Str::Rolling, "The dice are rolling…", "Кости катятся…"},
    {Str::ToPlay, "To play: {}", "Сыграть: {}"},
    {Str::TurnDone, "Turn played", "Ход сыгран"},
    {Str::Thinking, "Thinking…", "Думает…"},
    {Str::ComputerMoving, "Moving…", "Ходит…"},
    {Str::NoMoves, "no moves", "ходов нет"},
    {Str::NoMovesToast, "No moves — the turn passes", "Ходов нет — ход переходит"},
    {Str::PressNew, "N or New game to play again", "N — новая игра"},
    {Str::DemoNext, "A new game begins shortly", "Скоро новая партия"},
    {Str::Pips, "Pips {}  ·  Off {}", "Пипсы {}  ·  Сброшено {}"},
    {Str::Score, "Score {} : {}", "Счёт {} : {}"},
    {Str::PointsOne, "A single game, 1 point", "Обычная победа, 1 очко"},
    {Str::PointsMars, "Mars, 2 points", "Марс, 2 очка"},
    {Str::PointsKoks, "Koks, 3 points", "Кокс, 3 очка"},
    {Str::History, "Moves", "Ходы"},
    {Str::NoTurnsYet, "No moves yet", "Ходов пока нет"},
    {Str::NothingToUndo, "Nothing to undo", "Нечего отменять"},
    {Str::EnterFirst, "First bring in the checker from the bar", "Сначала введите шашку с бара"},
    {Str::HeadRule, "Only one checker may leave the head each turn", "С головы за ход можно снять только одну шашку"},
    {Str::CannotMove, "That checker cannot move now", "Эта шашка сейчас не может ходить"},
    {Str::HintText, "Hint: {}", "Подсказка: {}"},
    {Str::Bar, "bar", "бар"},
    {Str::Off, "off", "сброс"},
    {Str::Footer, "F1 help · Tab panel · F11 full screen", "F1 справка · Tab панель · F11 весь экран"},
    {Str::Controls, "Controls", "Управление"},
    {Str::KeyClick, "Click", "Щелчок"},
    {Str::KeyDrag, "Drag", "Перетаскивание"},
    {Str::KeyDoubleClick, "Double-click", "Двойной щелчок"},
    {Str::KeyRightClick, "Right-click  Backspace", "Правый щелчок  Backspace"},
    {Str::KeyRightDrag, "Right-drag", "Правой кнопкой"},
    {Str::KeyWheel, "Wheel", "Колесо"},
    {Str::HelpClick, "Choose a checker, then where it goes", "Выбрать шашку, затем куда"},
    {Str::HelpDrag, "Move a checker", "Переставить шашку"},
    {Str::HelpDoubleClick, "Play the higher die", "Сыграть большую кость"},
    {Str::HelpTakeBack, "Take back a step", "Вернуть шаг"},
    {Str::HelpRotate, "Turn the view", "Повернуть вид"},
    {Str::HelpZoom, "Zoom", "Приблизить"},
    {Str::HelpRoll, "Roll the dice; end the turn", "Бросить кости; закончить ход"},
    {Str::HelpHint, "Hint; again to play it", "Подсказка; ещё раз — сыграть её"},
    {Str::HelpUndo, "Undo the turn", "Отменить ход"},
    {Str::HelpAutoRoll, "Roll by itself", "Автобросок"},
    {Str::HelpCamera, "Change or reset the view", "Сменить или вернуть вид"},
    {Str::HelpNew, "New game", "Новая игра"},
    {Str::HelpGame, "Long or short nardy", "Длинные или короткие"},
    {Str::HelpMode, "Mode", "Режим"},
    {Str::HelpLevel, "Computer's level", "Уровень компьютера"},
    {Str::HelpTheme, "Theme", "Оформление"},
    {Str::HelpSound, "Sound on or off", "Звук"},
    {Str::HelpLanguage, "Language", "Язык"},
    {Str::HelpPanel, "Show or hide the panel", "Показать или скрыть панель"},
    {Str::HelpFullScreen, "Full screen", "Весь экран"},
    {Str::HelpClose, "Close this help", "Закрыть справку"},
    {Str::Rules, "Rules", "Правила"},
    {Str::RulesLong,
     "Long nardy. Each player starts with fifteen checkers on the head, and both run the same way round the board, "
     "from the head to their home and off. A checker moves the number of each die; a double is played four times. A "
     "single checker holds a point: you may not land where the opponent stands. Only one checker may leave the head "
     "a turn, two on a first roll of 6-6, 4-4 or 3-3. You may not hold six points in a row unless an opponent's "
     "checker is already past them. Play both dice if you can, else the higher one. Once all fifteen are home they "
     "are borne off; the first to bear off all wins, two points (mars) if the opponent has borne off none.",
     "Длинные нарды. У каждого пятнадцать шашек на «голове», и оба игрока идут по доске в одну сторону: от головы к "
     "своему дому и дальше за доску. Шашка ходит на число очков каждой кости, дубль играется четыре раза. Одна шашка "
     "уже занимает пункт: туда, где стоит соперник, ходить нельзя. С головы за ход снимается одна шашка, а в первый "
     "ход при 6-6, 4-4 и 3-3 — две. Нельзя ставить шесть пунктов подряд, если впереди них нет ни одной шашки "
     "соперника. Нужно сыграть обе кости, если можно, иначе большую. Когда все пятнадцать в доме, их выбрасывают; "
     "кто первым выбросил все — победил, а если у соперника нет ни одной выброшенной — марс, два очка."},
    {Str::RulesShort,
     "Short nardy (backgammon). The players run toward each other. A checker moves the number of each die; a double "
     "is played four times. A point with two or more opposing checkers is closed; a lone checker may be hit, and "
     "goes to the bar, from where it must enter the opponent's home before any other checker moves. Play both dice "
     "if you can, else the higher one. Once all fifteen are home they are borne off. A win is one point, mars (the "
     "opponent has borne off none) two, koks (and still has a checker on the bar or in the winner's home) three.",
     "Короткие нарды (бэкгаммон). Игроки идут навстречу друг другу. Шашка ходит на число очков каждой кости, дубль "
     "играется четыре раза. Пункт, где стоят две шашки соперника и больше, закрыт; одиночную шашку можно побить — "
     "она уходит на бар, и прежде чем ходить другими, её нужно ввести в дом соперника. Нужно сыграть обе кости, если "
     "можно, иначе большую. Когда все пятнадцать в доме, их выбрасывают. Победа — одно очко, марс (у соперника нет "
     "ни одной выброшенной) — два, кокс (и вдобавок его шашка на баре или в доме победителя) — три."},
}};

constexpr bool InOrder()
{
    for (std::size_t i = 0; i < Table.size(); ++i)
        if (std::to_underlying(Table[i].id) != static_cast<int>(i))
            return false;
    return true;
}
static_assert(InOrder(), "the table lists the strings in the order of Str");

} // namespace

std::string_view Tr(Str id, Language language)
{
    const Entry& entry = Table[static_cast<std::size_t>(std::to_underlying(id))];
    return language == Language::Russian ? entry.russian : entry.english;
}
